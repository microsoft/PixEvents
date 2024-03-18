// Copyright (C) Microsoft Corporation. All rights reserved.

namespace VisualStudioWorkarounds
{
    // In Visual Studio, the Test Explorer conveniently discovers all tests (VS Tests, Google Tests etc) in
    // the solution file.
    //
    // Unfortunately there is no easy way to hide certain tests by default. This is a problem, since some of
    // the PIX on Windows "functional" tests do complex things (e.g. create D3D12 devices) that may not work
    // well on PCs owned by other parts of the PIX team (e.g. the CPU team).
    //
    // We want those teams to be able to click "Run All Tests" from Visual Studio Test Explorer and see success,
    // without having to debug D3D12 issues (e.g. on old GPUs that we don't support).
    //
    // To do this, we hide the PIX on Windows "functional" tests from the Visual Studio Test Explorer for anyone
    // who's set the "DISABLE_GPU_FVT" environment variable.
    //
    // Note that we can still run the tests outside VS on anyone's PC simply by running the .exe.
    bool ShouldEarlyOutToHideFunctionalTestsFromTestExplorer(int argc, char** argv, char** envp)
    {
        if (argc >= 2 && std::string_view(argv[1]).find("gtest_list_tests") != std::string::npos)
        {
            bool isVisualStudioTestExplorerDiscoveringTests = false;
            bool isDisableFunctionalTestsEnvVariablePresent = false;

            for (char **env = envp; *env != 0; env++)
            {
                auto envView = std::string_view(*env);

                if (envView.find("VisualStudioVersion=") != std::string::npos)
                {
                    isVisualStudioTestExplorerDiscoveringTests = true;
                }
                else if (envView.find("DISABLE_GPU_FVT=") != std::string::npos)
                {
                    isDisableFunctionalTestsEnvVariablePresent = true;
                }
            }

            if (isVisualStudioTestExplorerDiscoveringTests && isDisableFunctionalTestsEnvVariablePresent)
            {
                return true; // Yes, hide the functional tests
            }
            else
            {
                return false; // No, show the functional tests in Test Explorer
            }
        }

        return false; // This isn't test discovery, so we shouldn't do anything
    }
}
