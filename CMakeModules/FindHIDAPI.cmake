find_library(HIDAPI_LIBRARY
  NAMES hidapi-hidraw hidapi-libusb hidapi
)

find_path(HIDAPI_INCLUDE_DIR
  NAMES hidapi.h
  PATH_SUFFIXES hidapi
)
