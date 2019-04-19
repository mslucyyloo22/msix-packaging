//
//  Copyright (C) 2019 Microsoft.  All rights reserved.
//  See LICENSE file in the project root for full license information.
// 
#include "AppxSignature.hpp"
#include "Exceptions.hpp"
#include "FileStream.hpp"
#include "SignatureCreator.hpp"
#include "MSIXResource.hpp"
#include "StreamHelper.hpp"

#include "SharedOpenSSL.hpp"

namespace MSIX
{
    namespace
    {
        int ParsePKCS12(PKCS12* p12, const char* pass, unique_EVP_PKEY& pkey, unique_X509& cert, unique_STACK_X509& ca)
        {
            EVP_PKEY* pkey_ = nullptr;
            X509* cert_ = nullptr;
            STACK_OF(X509)* ca_ = ca.get();

            int retVal = PKCS12_parse(p12, pass, &pkey_, &cert_, &ca_);

            // If ca existed, the certs will have been added to it and it is the same pointer.
            // If it is not set, a new stack will have been created and we need to set it out.
            if (!static_cast<bool>(ca))
            {
                ca.reset(ca_);
            }
            cert.reset(cert_);
            pkey.reset(pkey_);

            return retVal;
        }

        int PKCS7_indirect_data_content_new(PKCS7 *p7, const CustomOpenSSLObjects& customObjects)
        {
            PKCS7 *ret = NULL;

            if ((ret = PKCS7_new()) == NULL)
                goto err;

            ret->type = customObjects.Get(CustomOpenSSLObjectName::spcIndirectDataContext).GetObj();

            ret->d.other = ASN1_TYPE_new();
            if (!ret->d.other)
                goto err;

            if (!ASN1_TYPE_set_octetstring(ret->d.other, nullptr, 0))
                goto err;

            if (!PKCS7_set_content(p7, ret))
                goto err;

            return (1);
        err:
            if (ret != NULL)
                PKCS7_free(ret);
            return (0);
        }

        PKCS7 *PKCS7_sign_indirect_data(X509 *signcert, EVP_PKEY *pkey, STACK_OF(X509) *certs,
            BIO *data, int flags, const CustomOpenSSLObjects& customObjects)
        {
            PKCS7 *p7;
            int i;

            if (!(p7 = PKCS7_new())) {
                PKCS7err(PKCS7_F_PKCS7_SIGN, ERR_R_MALLOC_FAILURE);
                return NULL;
            }

            if (!PKCS7_set_type(p7, NID_pkcs7_signed))
                goto err;

            // Standard PKCS7_sign only supports NID_pkcs7_data, but we want SPC_INDIRECT_DATA_OBJID
            if (!PKCS7_indirect_data_content_new(p7, customObjects))
                goto err;

            // Force SHA256 for now
            PKCS7_SIGNER_INFO* signerInfo = PKCS7_sign_add_signer(p7, signcert, pkey, EVP_sha256(), flags);
            if (!signerInfo) {
                PKCS7err(PKCS7_F_PKCS7_SIGN, PKCS7_R_PKCS7_ADD_SIGNER_ERROR);
                goto err;
            }

            // Add our authenticated attributes
            PKCS7_add_attrib_content_type(signerInfo, customObjects.Get(CustomOpenSSLObjectName::spcIndirectDataContext).GetObj());

            // TODO: Make a cleaner way to generate this sequence for the statement type.
            const uint8_t statementType[]{ 0x30, 0x0C, 0x06, 0x0A, 0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x01, 0x15 };

            // TODO: smart pointer
            ASN1_STRING* statementString = ASN1_STRING_type_new(V_ASN1_SEQUENCE);
            ASN1_STRING_set(statementString, const_cast<uint8_t*>(statementType), sizeof(statementType));

            PKCS7_add_signed_attribute(signerInfo, customObjects.Get(CustomOpenSSLObjectName::spcStatementType).GetNID(), V_ASN1_SEQUENCE, statementString);

            // TODO: Make a cleaner way to generate this too
            const uint8_t opusType[]{ 0x30, 0x00 };

            // TODO: smart pointer
            ASN1_STRING* opusString = ASN1_STRING_type_new(V_ASN1_SEQUENCE);
            ASN1_STRING_set(opusString, const_cast<uint8_t*>(opusType), sizeof(opusType));

            PKCS7_add_signed_attribute(signerInfo, customObjects.Get(CustomOpenSSLObjectName::spcSpOpusInfo).GetNID(), V_ASN1_SEQUENCE, opusString);

            // Always include the chain certs
            for (i = 0; i < sk_X509_num(certs); i++) {
                if (!PKCS7_add_certificate(p7, sk_X509_value(certs, i)))
                    goto err;
            }

            if (!PKCS7_final(p7, data, flags))
            {
                goto err;
            }

            return p7;

        err:
            PKCS7_free(p7);
            return NULL;
        }
    }

    // [X] 1. Get self signed PKCS7_sign working
    // [X] 2. Try to hack the OID of the contents to not be 'data'
    // [ ] 3. If that makes a sufficiently nice looking output, create indirect data blob in ASN
    // [ ] 4. Else?
    ComPtr<AppxSignatureObject> SignatureCreator::Create(
        IAppxPackageReader* package)
    {
        OpenSSL_add_all_algorithms();
        CustomOpenSSLObjects customObjects{};

        //// TODO: Take in IStream for certificate
        //MSIX::ComPtr<IStream> certStream;
        //ThrowHrIfFailed(CreateStreamOnFile(R"(C:\Temp\evernotesup\cert.cer)", true, &certStream));
        //auto certBytes = Helper::CreateBufferFromStream(certStream);
        //
        //unique_BIO certBIO{ BIO_new_mem_buf(certBytes.data(), certBytes.size()) };
        //unique_X509 cert{ d2i_X509_bio(certBIO.get(), nullptr) };

        //// TODO: Take in IStream for private key
        //// TODO: Take in PFX as well
        //MSIX::ComPtr<IStream> privKeyStream;
        //ThrowHrIfFailed(CreateStreamOnFile(R"(C:\Temp\evernotesup\cert.pvk)", true, &privKeyStream));
        //auto privKeyBytes = Helper::CreateBufferFromStream(privKeyStream);

        //unique_BIO privKeyBIO{ BIO_new_mem_buf(privKeyBytes.data(), privKeyBytes.size()) };
        //unique_EVP_PKEY privKey{ b2i_PVK_bio(privKeyBIO.get(), nullptr, nullptr) };

        // Create the p7s out of the target p7x
        MSIX::ComPtr<IStream> p7xStream;
        ThrowHrIfFailed(CreateStreamOnFile(R"(C:\Temp\evernotesup\AppxSignatureSelfSign.p7x)", true, &p7xStream));
        auto p7xBytes = Helper::CreateBufferFromStream(p7xStream);

        // Write out the p7s
        MSIX::ComPtr<IStream> p7sStream;
        ThrowHrIfFailed(CreateStreamOnFile(R"(C:\Temp\evernotesup\original.p7s)", false, &p7sStream));
        p7sStream->Write(&p7xBytes[4], p7xBytes.size() - 4, nullptr);

        // Grab the target bytes for use as data
        const size_t offsetToData = 67;
        const size_t lengthOfData = 264;
        const size_t offsetOfSignedPortion = 4;
        const uint8_t* targetSequence = &p7xBytes[offsetToData];

        MSIX::ComPtr<IStream> certStream;
        ThrowHrIfFailed(CreateStreamOnFile(R"(C:\Temp\evernotesup\cert.pfx)", true, &certStream));
        auto certBytes = Helper::CreateBufferFromStream(certStream);
        
        unique_BIO certBIO{ BIO_new_mem_buf(certBytes.data(), certBytes.size()) };
        unique_PKCS12 cert{ d2i_PKCS12_bio(certBIO.get(), nullptr) };

        unique_EVP_PKEY privKey;
        unique_X509 leaf;
        unique_STACK_X509 chain;

        CheckOpenSSLErr(ParsePKCS12(cert.get(), nullptr, privKey, leaf, chain));

        unique_BIO dataBIO{ BIO_new_mem_buf(targetSequence + offsetOfSignedPortion, lengthOfData - offsetOfSignedPortion) };

        unique_PKCS7 p7{ PKCS7_sign_indirect_data(leaf.get(), privKey.get(), chain.get(), dataBIO.get(), PKCS7_BINARY | PKCS7_NOATTR, customObjects) };
        CheckOpenSSLAlloc(p7);

        // Overwrite the signed contents with the complete one including the additional sequence at the beginning
        ASN1_STRING* sequenceString = ASN1_STRING_type_new(V_ASN1_SEQUENCE);
        ASN1_STRING_set(sequenceString, const_cast<uint8_t*>(targetSequence), lengthOfData);

        ASN1_TYPE_set(p7->d.sign->contents->d.other, V_ASN1_SEQUENCE, sequenceString);

        unique_BIO outBIO{ BIO_new(BIO_s_mem()) };
        i2d_PKCS7_bio(outBIO.get(), p7.get());

        MSIX::ComPtr<IStream> outStream;
        ThrowHrIfFailed(CreateStreamOnFile(R"(C:\Temp\evernotesup\openssltest.p7s)", false, &outStream));

        char* out = nullptr;
        long cOut = BIO_get_mem_data(outBIO.get(), &out);

        outStream->Write(out, cOut, nullptr);

        MSIX::ComPtr<IStream> p7xOutStream;
        ThrowHrIfFailed(CreateStreamOnFile(R"(C:\Temp\evernotesup\openssltest.p7x)", false, &p7xOutStream));

        uint32_t prefix = P7X_FILE_ID;
        p7xOutStream->Write(&prefix, sizeof(prefix), nullptr);
        p7xOutStream->Write(out, cOut, nullptr);

        return {};
    }
} // namespace MSIX
