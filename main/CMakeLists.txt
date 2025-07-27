idf_component_register(SRCS "background_task.cc" "application.cc" "bsp/bsp_ir_encoder.cc" 
 "main.cc" "bsp/bsp_sht3x.cc" "protocol/mqtt_protocol.cc" "protocol/ir_kelon.cc"
                    INCLUDE_DIRS "." "bsp" "protocol"
                    WHOLE_ARCHIVE
                    )
