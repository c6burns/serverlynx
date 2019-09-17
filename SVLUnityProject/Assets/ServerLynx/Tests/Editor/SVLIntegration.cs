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

using NUnit.Framework;
using System;
using System.Net;

public unsafe class SVLIntegration
{
    const ushort LISTEN_PORT = 52343;

    ushort _port = SL.Util.HtoN(LISTEN_PORT);

    [OneTimeSetUp]
    public void FixtureSetup()
    {
    }

    [OneTimeTearDown]
    public void FixtureCleanup()
    {
    }

    [SetUp]
    public void TestSetup()
    {
    }

    [TearDown]
    public void TestCleanup()
    {
    }

    [Test]
    public void SetupCleanup()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void SetupCleanupTwice()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void DoubleSetupCleanup()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void SetupDoubleCleanup()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void DoubleSetupDoubleCleanup()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void StartStop()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Start(service));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void StartStopTwice()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Start(service));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));

        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Start(service));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void DoubleStartStop()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Start(service));
        Assert.True(SVL.API.Start(service));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void StartDoubleStop()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Start(service));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void DoubleStartDoubleStop()
    {
        void* service = null;
        SL.C.Context ctx = default;
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Start(service));
        Assert.True(SVL.API.Start(service));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void StartListenStop()
    {
        void* service = null;
        SL.C.Context ctx = default;
        SL.C.Endpoint local_endpoint = SL.C.Endpoint.NewV4(&ctx, _port, SL.C.IPv4.New(127, 0, 0, 1));
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Start(service, &local_endpoint));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }

    [Test]
    public void StartListenStopTwice()
    {
        void* service = null;
        SL.C.Context ctx = default;
        SL.C.Endpoint local_endpoint = SL.C.Endpoint.NewV4(&ctx, _port, SL.C.IPv4.New(127, 0, 0, 1));
        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Start(service, &local_endpoint));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));

        Assert.True(SVL.API.Setup(&ctx, &service));
        Assert.True(SVL.API.Start(service, &local_endpoint));
        Assert.True(SVL.API.Stop(service));
        Assert.True(SVL.API.Cleanup(&ctx, &service));
    }
}
