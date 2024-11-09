# JVME: JLab API for userspace access to the VME bus.



### Quick Start (Linux i686 or x86_64)
1. Set Environment Variables

 + bash Example

     ```Bash
     export LINUXVME=${HOME}/linuxvme
     export LINUXVME_INC=${LINUXVME}/include
     export LINUXVME_LIB=${LINUXVME}/lib
     export LINUXVME_BIN=${LINUXVME}/bin
     export LD_LIBRARY_PATH=${LINUXVME_LIB}:${LD_LIBRARY_PATH}
     ```
 + csh Example

     ```Tcsh
     setenv LINUXVME ${HOME}/linuxvme
     setenv LINUXVME_INC ${LINUXVME}/include
     setenv LINUXVME_LIB ${LINUXVME}/lib
     setenv LINUXVME_BIN ${LINUXVME}/bin
     setenv LD_LIBRARY_PATH ${LINUXVME_LIB}:${LD_LIBRARY_PATH}
     ```

2. Compile and install JVME library

      ```ShellSession
      $ make -B install
      ```

3. Compile and install VME debugging programs (optional)
     ```ShellSession
     $ cd src/
     $ make -B install
     ```
