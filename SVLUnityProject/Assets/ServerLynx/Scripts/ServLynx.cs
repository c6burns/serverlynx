/*
 * Copyright (c) 2019 Chris Burns <chris@kitty.city>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

#if UNITY_EDITOR || SL_TESTING_ENABLED || SVL_TESTING_ENABLED
[assembly: InternalsVisibleTo("CSharpServLynxTests")]
#endif

namespace SVL
{
    public unsafe static class API
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool Setup(SL.C.Context* ctx, void** service)
        {
            return (SL.C.servlynx_setup(ctx, service) == SL.C.SL_OK);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool Cleanup(SL.C.Context* ctx, void** service)
        {
            return (SL.C.servlynx_cleanup(ctx, service) == SL.C.SL_OK);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool Start(void* service, SL.C.Endpoint* endpoint = null)
        {
            return (SL.C.servlynx_start(service, endpoint) == SL.C.SL_OK);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool Stop(void* service)
        {
            return (SL.C.servlynx_stop(service) == SL.C.SL_OK);
        }

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        //public static bool SocketOpen(SL.C.Socket* sock)
        //{
        //    return (SL.C.socklynx_socket_open(sock) == SL.C.SL_OK);
        //}

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        //public static bool SocketClose(SL.C.Socket* sock)
        //{
        //    return (SL.C.socklynx_socket_close(sock) == SL.C.SL_OK);
        //}

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        //public static int SocketSend(SL.C.Socket* sock, SL.C.Buffer* bufferArray, int bufferCount, SL.C.Endpoint* endpoint)
        //{
        //    return SL.C.socklynx_socket_send(sock, bufferArray, bufferCount, endpoint);
        //}

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        //public static int SocketRecv(SL.C.Socket* sock, SL.C.Buffer* bufferArray, int bufferCount, SL.C.Endpoint* endpoint)
        //{
        //    return SL.C.socklynx_socket_recv(sock, bufferArray, bufferCount, endpoint);
        //}
    }
}
