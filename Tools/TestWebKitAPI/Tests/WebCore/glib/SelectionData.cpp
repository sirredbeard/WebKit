/*
 * Copyright (C) 2026 Hayden Barnes. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if PLATFORM(GTK) || PLATFORM(WPE)

#include "Helpers/Test.h"

#include <WebCore/SelectionData.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {
using namespace WebCore;

// Layer 1: web-authored uri-list must not become content-granting filenames.
// External/trusted callers opt in via setFilenames*.

TEST(SelectionData, SetURIListDoesNotPromoteFilenames)
{
    SelectionData data;
    data.setURIList("file:///etc/passwd\r\nhttps://example.com/\r\n"_s);

    EXPECT_TRUE(data.hasURIList());
    EXPECT_TRUE(data.hasURL());
    EXPECT_EQ(data.url().string(), "file:///etc/passwd"_s);
    EXPECT_FALSE(data.hasFilenames());
    EXPECT_TRUE(data.filenames().isEmpty());
}

TEST(SelectionData, SetURIListKeepsHttpURLWithoutFilenames)
{
    SelectionData data;
    data.setURIList("https://example.com/path\n"_s);

    EXPECT_TRUE(data.hasURL());
    EXPECT_EQ(data.url().string(), "https://example.com/path"_s);
    EXPECT_FALSE(data.hasFilenames());
}

TEST(SelectionData, TrustedSetFilenamesFromURIList)
{
    SelectionData data;
    data.setURIList("file:///tmp/trusted.txt\r\nhttps://example.com/\r\n"_s);
    EXPECT_FALSE(data.hasFilenames());

    data.setFilenamesFromURIList(data.uriList());
    EXPECT_TRUE(data.hasFilenames());
    ASSERT_EQ(data.filenames().size(), 1u);
    EXPECT_EQ(data.filenames()[0], "/tmp/trusted.txt"_s);
}

TEST(SelectionData, ExplicitSetFilenames)
{
    SelectionData data;
    data.setURIList("file:///tmp/ignored-for-files.txt"_s);
    data.setFilenames(Vector<String> { "/home/user/document.pdf"_s, "/home/user/photo.png"_s });

    ASSERT_EQ(data.filenames().size(), 2u);
    EXPECT_EQ(data.filenames()[0], "/home/user/document.pdf"_s);
    EXPECT_EQ(data.filenames()[1], "/home/user/photo.png"_s);
}

TEST(SelectionData, FilenamesFromURIListSkipsCommentsAndNonFiles)
{
    auto filenames = SelectionData::filenamesFromURIList(
        "# comment\n"
        "https://example.com/a\n"
        "file:///tmp/a.txt\n"
        "\n"
        "file:///tmp/b.txt\r\n"_s);

    ASSERT_EQ(filenames.size(), 2u);
    EXPECT_EQ(filenames[0], "/tmp/a.txt"_s);
    EXPECT_EQ(filenames[1], "/tmp/b.txt"_s);
}

TEST(SelectionData, ClearFilenames)
{
    SelectionData data;
    data.setFilenames(Vector<String> { "/tmp/x"_s });
    EXPECT_TRUE(data.hasFilenames());
    data.clearFilenames();
    EXPECT_FALSE(data.hasFilenames());
}

} // namespace TestWebKitAPI

#endif // PLATFORM(GTK) || PLATFORM(WPE)
