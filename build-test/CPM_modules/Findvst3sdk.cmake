include("/home/alexis/dev/silvertune/build-test/cmake/CPM_0.40.2.cmake")
CPMAddPackage("NAME;vst3sdk;GITHUB_REPOSITORY;steinbergmedia/vst3sdk;GIT_TAG;v3.8.0_build_66;EXCLUDE_FROM_ALL;TRUE;DOWNLOAD_ONLY;TRUE;GIT_SUBMODULES;base;public.sdk;pluginterfaces;cmake;SOURCE_DIR;cpm/vst3sdk")
set(vst3sdk_FOUND TRUE)