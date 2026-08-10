/*
 * Copyright (C) 2026 Hayden Barnes
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <utility>

namespace WebKit {

// Bookkeeping for the asynchronous GTK4 drop lifecycle.
//
// GtkDropTargetAsync transfers ownership of the GdkDrop to us as soon as the ::drop
// handler returns TRUE, and from that moment the drop target owes the drag source
// exactly one gdk_drop_finish(). The data for the offered mime types is read
// asynchronously, so a drop can arrive before SelectionData is complete; completing
// it has to wait. That makes the finish obligation outlive the ::drop handler, so it
// must also be honoured by the teardown paths (drag-leave, a replacement drop and
// widget destruction), otherwise the source is left waiting forever.
//
// This is deliberately free of GTK types so the lifecycle rules can be tested
// directly. DropTargetGtk4 owns one of these and never tracks the state separately.
class DropTargetState {
public:
    // A drop was accepted and its data reads (if any) were scheduled.
    void didAccept(bool waitingForData)
    {
        m_waitingForData = waitingForData;
        m_deferredDrop = false;
        m_unfinishedDrop = false;
    }

    // Every asynchronous read scheduled by didAccept() has completed.
    void didFinishLoadingData() { m_waitingForData = false; }

    // GTK handed us the drop. Returns true when it has to be deferred until the data
    // finishes loading, in which case takeDeferredDrop() completes it later.
    bool didRequestDrop()
    {
        m_unfinishedDrop = true;
        if (m_waitingForData)
            m_deferredDrop = true;
        return m_deferredDrop;
    }

    // Returns true once for a drop that didRequestDrop() deferred, meaning the caller
    // should now perform it.
    bool takeDeferredDrop()
    {
        return std::exchange(m_deferredDrop, false);
    }

    // The drop was performed and gdk_drop_finish() was called for it.
    void didFinishDrop()
    {
        m_deferredDrop = false;
        m_unfinishedDrop = false;
    }

    // Returns true once when a drop was taken over but never finished, meaning the
    // caller still has to call gdk_drop_finish() to release the drag source.
    bool takeUnfinishedDrop()
    {
        m_waitingForData = false;
        m_deferredDrop = false;
        return std::exchange(m_unfinishedDrop, false);
    }

    bool isWaitingForData() const { return m_waitingForData; }

private:
    bool m_waitingForData { false };
    bool m_deferredDrop { false };
    bool m_unfinishedDrop { false };
};

} // namespace WebKit
