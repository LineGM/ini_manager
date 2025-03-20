# ---- Check if Doxygen is installed  ----

find_package(Doxygen)
if(DOXYGEN_FOUND)
	set(DOXYGEN_IN ${PROJECT_SOURCE_DIR}/docs/Doxyfile.in)
	set(DOXYGEN_OUT ${PROJECT_SOURCE_DIR}/docs/Doxyfile)
	configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

	set(working_dir "${PROJECT_SOURCE_DIR}/docs")

	# ---- Declare documentation target ----

	add_custom_target(
		docs
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
		COMMENT "Building documentation using Doxygen."
		WORKING_DIRECTORY "${working_dir}"
	)

else(DOXYGEN_FOUND)
	message("Doxygen must be installed to build documentation.")
endif(DOXYGEN_FOUND)
