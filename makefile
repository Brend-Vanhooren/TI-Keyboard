# ----------------------------
# Makefile Options
# ----------------------------

NAME = Keyboard
ICON = key.png
DESCRIPTION = "TI-84 as USB Keyboard"
COMPRESSED = NO
ARCHIVED = YES

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
