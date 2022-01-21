idf_component_register(SRCS "main.c"
		                    "server.c"
        		            "connect.c"
                		    "gpio_ex.c"
                		    INCLUDE_DIRS ".")
spiffs_create_partition_image(spiffs ../html FLASH_IN_PROJECT)


#set(COMPONENT_ADD_INCLUDEDIRS ".")

#set(COMPONENT_EMBED_TXTFILES
#        "../client/esp32-demo/build/index.html")

#register_component()