# escape=`

FROM lamanchy/wolf_base

# prepare wolf app
RUN cp -rf /wolf_lib/examples/app /wolf && `
# configure release
    mkdir /wolf-build && `
    cd /wolf-build && `
    cmake -DWOLF_PATH=/wolf_lib -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/wolf/install/linux-docker -G "CodeBlocks - Unix Makefiles" /wolf && `
    # remove downloaded files to save space
    rm -rf wolf_lib/lib_source && `
# build release
    cmake --build . --target install -- -j 4

SHELL ["/bin/bash", "-c"]
CMD echo 'Initializing wolf app?' && `
    if [ -z "$(ls -A /wolf)" ]; then echo True; cp -rf /wolf_lib/examples/app/* /wolf; else echo False; fi && `
    cd /wolf-build && `
    cmake --build . --target install -- -j 4


