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

#include <WebCore/DragData.h>
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

TEST(SelectionData, URIListWithoutFilenamesStripsFileURLs)
{
    auto sanitized = SelectionData::uriListWithoutFilenames(
        "file:///etc/passwd\r\nhttps://example.com/a\r\nfile:///tmp/x\r\n"_s);
    EXPECT_EQ(sanitized, "https://example.com/a"_s);
}

TEST(SelectionData, URIListWithoutFilenamesEmptyWhenOnlyFiles)
{
    auto sanitized = SelectionData::uriListWithoutFilenames("file:///tmp/only\n"_s);
    EXPECT_TRUE(sanitized.isEmpty());
}

// Mirrors SelectionData.serialization.in decode: filenames survive without setURIList promotion.
TEST(SelectionData, IpcConstructorPreservesFilenamesWithoutURIListPromotion)
{
    SelectionData decoded(
        String(),
        String(),
        URL(),
        "file:///etc/passwd\r\nhttps://example.com/\r\n"_s,
        Vector<String> { "/tmp/trusted-drop.txt"_s },
        nullptr,
        nullptr,
        false);

    EXPECT_TRUE(decoded.hasURIList());
    EXPECT_TRUE(decoded.hasFilenames());
    ASSERT_EQ(decoded.filenames().size(), 1u);
    EXPECT_EQ(decoded.filenames()[0], "/tmp/trusted-drop.txt"_s);

    SelectionData uriOnly(
        String(),
        String(),
        URL(),
        "file:///etc/passwd\r\n"_s,
        Vector<String> { },
        nullptr,
        nullptr,
        false);
    EXPECT_TRUE(uriOnly.hasURIList());
    EXPECT_FALSE(uriOnly.hasFilenames());
}

TEST(SelectionData, DragDataIsSourceDeniesFilenameAccess)
{
    SelectionData selection;
    selection.setFilenames(Vector<String> { "/tmp/trusted-looking.txt"_s });

    DragData external(&selection, { }, { }, { });
    EXPECT_TRUE(external.containsFiles());
    EXPECT_EQ(external.numberOfFiles(), 1u);

    DragData local(&selection, { }, { }, { }, DragApplicationFlags::IsSource);
    EXPECT_FALSE(local.containsFiles());
    EXPECT_EQ(local.numberOfFiles(), 0u);
    EXPECT_TRUE(local.asFilenames().isEmpty());
}

// Portal / trusted list wins over a parallel hostile uri-list (DropTargetGtk4 policy).
// UIProcess sets filenames from portal only; uri-list may still carry strings.
TEST(SelectionData, PortalFilenamesNotWidenedByHostileURIList)
{
    SelectionData data;
    data.setURIList("file:///etc/passwd\r\nhttps://example.com/\r\nfile:///tmp/extra-from-uri-list.txt\r\n"_s);
    data.setFilenames(Vector<String> { "/run/user/1000/doc/portal-only.txt"_s });

    ASSERT_EQ(data.filenames().size(), 1u);
    EXPECT_EQ(data.filenames()[0], "/run/user/1000/doc/portal-only.txt"_s);
    EXPECT_FALSE(data.filenames().contains("/etc/passwd"_s));
    EXPECT_FALSE(data.filenames().contains("/tmp/extra-from-uri-list.txt"_s));
    EXPECT_TRUE(data.hasURIList());
}

// Export sanitize contract used by DragSource and clipboard write.
TEST(SelectionData, UriListWithoutFilenamesKeepsHttpOnly)
{
    auto out = SelectionData::uriListWithoutFilenames(
        "file:///etc/passwd\r\nhttps://example.com/a\r\nhttp://example.org/b\r\nfile:///tmp/x\r\n"_s);
    EXPECT_TRUE(out.contains("https://example.com/a"_s));
    EXPECT_TRUE(out.contains("http://example.org/b"_s));
    EXPECT_FALSE(out.contains("file://"_s));
    EXPECT_FALSE(out.contains("passwd"_s));
}

// Trusted external drop shape after IPC: filenames present, uri-list may list files,
// containsFiles true only when not IsSource.
TEST(SelectionData, TrustedDropShapeAfterIpcRoundTrip)
{
    SelectionData decoded(
        String(),
        String(),
        URL(),
        "file:///home/user/doc.txt\r\n"_s,
        Vector<String> { "/home/user/doc.txt"_s },
        nullptr,
        nullptr,
        false);

    EXPECT_TRUE(decoded.hasFilenames());
    DragData external(&decoded, { }, { }, { });
    EXPECT_TRUE(external.containsFiles());
    EXPECT_EQ(external.numberOfFiles(), 1u);

    DragData asSource(&decoded, { }, { }, { }, DragApplicationFlags::IsSource);
    EXPECT_FALSE(asSource.containsFiles());
}

// setTrustedDrop is the single entry point UIProcess drop targets use, so the
// portal-wins rule cannot drift between the GTK3 and GTK4 implementations.

TEST(SelectionData, TrustedDropPrefersPortalFilenames)
{
    SelectionData data;
    data.setTrustedDrop("file:///etc/passwd\r\nfile:///tmp/extra.txt\r\n"_s,
        Vector<String> { "/run/user/1000/doc/portal-only.txt"_s });

    ASSERT_EQ(data.filenames().size(), 1u);
    EXPECT_EQ(data.filenames()[0], "/run/user/1000/doc/portal-only.txt"_s);
    EXPECT_FALSE(data.filenames().contains("/etc/passwd"_s));
    EXPECT_FALSE(data.filenames().contains("/tmp/extra.txt"_s));
}

TEST(SelectionData, TrustedDropFallsBackToURIListWhenNoPortalList)
{
    SelectionData data;
    data.setTrustedDrop("file:///home/user/a.txt\r\nfile:///home/user/b.txt\r\nhttps://example.com/\r\n"_s, { });

    ASSERT_EQ(data.filenames().size(), 2u);
    EXPECT_EQ(data.filenames()[0], "/home/user/a.txt"_s);
    EXPECT_EQ(data.filenames()[1], "/home/user/b.txt"_s);
    EXPECT_TRUE(data.hasURIList());
}

TEST(SelectionData, TrustedDropWithoutFilesGrantsNothing)
{
    SelectionData data;
    data.setTrustedDrop("https://example.com/\r\n"_s, { });

    EXPECT_FALSE(data.hasFilenames());
    EXPECT_TRUE(data.hasURL());
}

TEST(SelectionData, TrustedDropWithOnlyPortalListHasNoURIList)
{
    SelectionData data;
    data.setTrustedDrop(emptyString(), Vector<String> { "/run/user/1000/doc/only.txt"_s });

    ASSERT_EQ(data.filenames().size(), 1u);
    EXPECT_EQ(data.filenames()[0], "/run/user/1000/doc/only.txt"_s);
    EXPECT_FALSE(data.hasURIList());
}

// A second drop must not inherit the previous grant.
TEST(SelectionData, TrustedDropReplacesPreviousFilenames)
{
    SelectionData data;
    data.setTrustedDrop("file:///home/user/first.txt\r\n"_s, { });
    ASSERT_EQ(data.filenames().size(), 1u);

    data.setTrustedDrop("https://example.com/\r\n"_s, { });
    EXPECT_FALSE(data.hasFilenames());
}

} // namespace TestWebKitAPI

#endif // PLATFORM(GTK) || PLATFORM(WPE)
