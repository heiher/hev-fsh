/*
 ============================================================================
 Name        : hev-bridge.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2026 hev
 Description : Bridge Win32 handles to a socket
 ============================================================================
 */

#ifndef __HEV_BRIDGE_H__
#define __HEV_BRIDGE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Win32 pipes, files and consoles cannot be polled by the task system. Move
 * their data through worker threads and a socket pair instead: @in is read and
 * sent to the returned descriptor, what is received from it is written to
 * @out. Either handle may be NULL. If @flags has HEV_BRIDGE_OWN, the handles
 * are closed when the last worker thread ends, which happens when they fail or
 * the returned descriptor is closed. If it has HEV_BRIDGE_LF2CR, a line feed
 * written to @out that is not part of a CRLF becomes a carriage return, as
 * the Enter key of a pseudo console. If it has HEV_BRIDGE_DRAIN, @in is read until
 * it fails even if the returned descriptor is closed, so that a producer like
 * a pseudo console is never blocked on a full pipe.
 *
 * Returns: the non-blocking descriptor for the task system, or -1 on error.
 */
#define HEV_BRIDGE_OWN (1 << 0)
#define HEV_BRIDGE_LF2CR (1 << 1)
#define HEV_BRIDGE_DRAIN (1 << 2)

int hev_bridge_new (void *in, void *out, int flags);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_BRIDGE_H__ */
