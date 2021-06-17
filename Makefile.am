
bin_PROGRAMS = pcie_transcode

AM_DEFAULT_SOURCE_EXT = .c

pcie_transcode_INC_DIR = ./include
pcie_transcode_SRC_DIR = ./src

#NOTE: In some build system, single $ character is not interpreted correctly and removed, so we have added $$ to overcome such issue.
PCI_CFLAGS_PKG_CONFIG = $$(pkg-config --cflags  gstreamer-1.0 gstreamer-plugins-base-1.0 libdrm libv4l2 libmediactl pcie-lib)
PCI_LIBS_PKG_CONFIG = $$(pkg-config --libs gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-video-1.0 libdrm libv4l2 libmediactl pcie-lib)

pcie_transcode_SOURCES = \
			$(pcie_transcode_SRC_DIR)/pcie_main.c \
			$(pcie_transcode_SRC_DIR)/pcie_src.c

nodist_pcie_transcode_SOURCES = \
		$(pcie_transcode_INC_DIR)/pcie_main.h \
		$(pcie_transcode_INC_DIR)/pcie_src.h

pcie_transcode_CFLAGS = $(PCI_CFLAGS_PKG_CONFIG) -I$(pcie_transcode_INC_DIR)

pcie_transcode_LDFLAGS = $(PCI_LIBS_PKG_CONFIG) -lm
