// **********************************************************************
//
// Copyright (c) 2003-2017 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package test.Ice.facets;

public class Collocated extends test.Util.Application
{
    @Override
    public int run(String[] args)
    {
        Ice.Communicator communicator = communicator();
        communicator.getProperties().setProperty("TestAdapter.Endpoints", getTestEndpoint(0));
        Ice.ObjectAdapter adapter = communicator.createObjectAdapter("TestAdapter");
        Ice.Object d = new DI();
        adapter.add(d, Ice.Util.stringToIdentity("d"));
        adapter.addFacet(d, Ice.Util.stringToIdentity("d"), "facetABCD");
        Ice.Object f = new FI();
        adapter.addFacet(f, Ice.Util.stringToIdentity("d"), "facetEF");
        Ice.Object h = new HI(communicator);
        adapter.addFacet(h, Ice.Util.stringToIdentity("d"), "facetGH");

        AllTests.allTests(this);

        return 0;
    }

    @Override
    protected Ice.InitializationData getInitData(Ice.StringSeqHolder argsH)
    {
        Ice.InitializationData initData = super.getInitData(argsH);
        initData.properties.setProperty("Ice.Package.Test", "test.Ice.facets");
        return initData;
    }

    public static void main(String[] args)
    {
        Collocated app = new Collocated();
        int result = app.main("Collocated", args);
        System.gc();
        System.exit(result);
    }
}
