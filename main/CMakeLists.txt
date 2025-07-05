idf_component_register(SRCS "background_task.cc" "application.cc" "main.cc" "bsp/bsp_sht3x.cc" "protocol/mqtt_protocol.cc"
                    INCLUDE_DIRS "." "bsp" "protocol"
                    WHOLE_ARCHIVE
                    )
