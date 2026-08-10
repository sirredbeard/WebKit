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

#include "config.h"

#if PLATFORM(GTK)

#include "Helpers/Test.h"

#include <WebKit/UIProcess/API/gtk/DropTargetState.h>

namespace TestWebKitAPI {

using WebKit::DropTargetState;

// A drop that arrives after the data finished loading runs straight away and is
// finished by DropTarget::drop() itself.
TEST(DropTargetState, SynchronousDropIsNotDeferred)
{
    DropTargetState state;
    state.didAccept(true);
    state.didFinishLoadingData();

    EXPECT_FALSE(state.isWaitingForData());
    EXPECT_FALSE(state.didRequestDrop());
    EXPECT_FALSE(state.takeDeferredDrop());

    state.didFinishDrop();
    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// A drop that arrives while the mime loads are still running has to wait, and is
// then handed back exactly once when the data is ready.
TEST(DropTargetState, DropDuringLoadIsDeferredAndReplayedOnce)
{
    DropTargetState state;
    state.didAccept(true);

    EXPECT_TRUE(state.isWaitingForData());
    EXPECT_TRUE(state.didRequestDrop());

    state.didFinishLoadingData();
    EXPECT_TRUE(state.takeDeferredDrop());
    EXPECT_FALSE(state.takeDeferredDrop());

    state.didFinishDrop();
    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// A drop with nothing to load must not be deferred: didLoadData() never runs, so
// nothing would ever complete it.
TEST(DropTargetState, DropIsNotDeferredWhenNothingIsLoading)
{
    DropTargetState state;
    state.didAccept(false);

    EXPECT_FALSE(state.isWaitingForData());
    EXPECT_FALSE(state.didRequestDrop());
    EXPECT_FALSE(state.takeDeferredDrop());
}

// drag-leave cancels the reads, so the deferred drop can never be replayed. The
// GdkDrop still has to be finished or the drag source waits forever.
TEST(DropTargetState, LeaveAfterDeferredDropStillOwesFinish)
{
    DropTargetState state;
    state.didAccept(true);
    EXPECT_TRUE(state.didRequestDrop());

    EXPECT_TRUE(state.takeUnfinishedDrop());
    // Only once: leave() must not finish the same GdkDrop twice.
    EXPECT_FALSE(state.takeUnfinishedDrop());
    EXPECT_FALSE(state.takeDeferredDrop());
    EXPECT_FALSE(state.isWaitingForData());
}

// A drag-leave without any drop owes nothing; the drag source keeps the drop.
TEST(DropTargetState, LeaveWithoutDropOwesNothing)
{
    DropTargetState state;
    state.didAccept(true);

    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// The obligation is cleared as soon as the drop is finished, so a later leave or
// destruction does not double finish it.
TEST(DropTargetState, FinishedDropIsNotFinishedAgain)
{
    DropTargetState state;
    state.didAccept(true);
    state.didFinishLoadingData();
    state.didRequestDrop();
    state.didFinishDrop();

    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// Destruction while a deferred drop is outstanding still owes a finish.
TEST(DropTargetState, DestroyWithDeferredDropOwesFinish)
{
    DropTargetState state;
    state.didAccept(true);
    state.didRequestDrop();

    EXPECT_TRUE(state.takeUnfinishedDrop());
}

// Accepting a new drop starts from a clean slate; the caller settles the previous
// obligation before calling didAccept().
TEST(DropTargetState, AcceptResetsPreviousState)
{
    DropTargetState state;
    state.didAccept(true);
    state.didRequestDrop();
    EXPECT_TRUE(state.takeUnfinishedDrop());

    state.didAccept(true);
    EXPECT_TRUE(state.isWaitingForData());
    EXPECT_FALSE(state.takeDeferredDrop());
    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// Data finishing loading on its own does not invent a drop.
TEST(DropTargetState, LoadingCompletionWithoutDropDoesNotDefer)
{
    DropTargetState state;
    state.didAccept(true);
    state.didFinishLoadingData();

    EXPECT_FALSE(state.takeDeferredDrop());
    EXPECT_FALSE(state.takeUnfinishedDrop());
}

} // namespace TestWebKitAPI

#endif // PLATFORM(GTK)
