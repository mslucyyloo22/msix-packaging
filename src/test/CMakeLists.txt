# MSIX\src\test
# Copyright (C) 2017 Microsoft.  All rights reserved.
# See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 3.8.0 FATAL_ERROR)

# Copy the test packages to the test output directory to don't have to use
# ..\..\src\test\testData\<path>
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/testData" DESTINATION "${MSIX_TEST_OUTPUT_DIRECTORY}")

# Used for flat tests. Remove assets.appx to make it invalid
file(COPY "${MSIX_TEST_OUTPUT_DIRECTORY}/testData/unpack/flat/"
    DESTINATION "${MSIX_TEST_OUTPUT_DIRECTORY}/testData/unpack/badflat/"
    FILES_MATCHING
        PATTERN "*"
        PATTERN "assets*" EXCLUDE
)

add_subdirectory(msixtest)
