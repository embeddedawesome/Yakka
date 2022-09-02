
ifeq ($(OS),Windows_NT)
    ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
        HOST_OS := windows
        include $(BOB_MAKEFILE_DIRECTORY)/hosts/windows_host.mk
    else
        ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
            HOST_OS := windows
            include $(BOB_MAKEFILE_DIRECTORY)/hosts/windows_host.mk
        endif
        ifeq ($(PROCESSOR_ARCHITECTURE),x86)
            HOST_OS := win32
            include $(BOB_MAKEFILE_DIRECTORY)/hosts/windows_host.mk
        endif
    endif
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        HOST_OS := linux
        include $(BOB_MAKEFILE_DIRECTORY)/hosts/linux_host.mk
        
        UNAME_P := $(shell uname -p)
  		ifeq ($(UNAME_P),x86_64)
	        HOST_OS :=$(HOST_OS)64
	    endif
	    ifneq ($(filter %86 unknown,$(UNAME_P)),)
	        HOST_OS :=$(HOST_OS)32
	    endif
	    ifneq ($(filter arm%,$(UNAME_P)),)
	        HOST_OS :=$(HOST_OS)_arm
	    endif
    endif
    ifeq ($(UNAME_S),Darwin)
        HOST_OS := macos
        include $(BOB_MAKEFILE_DIRECTORY)/hosts/macos_host.mk
    endif
endif