set(srcs "main.c"
         "gatt_svr.c"
         "crc8.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ".")
