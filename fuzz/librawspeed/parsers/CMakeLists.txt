add_custom_target(ParserFuzzers ALL)
add_dependencies(fuzzers ParserFuzzers)

function(add_parser_fuzzer parser variant boolvar)
  set(fuzzer "${parser}Fuzzer${variant}")
  add_executable(${fuzzer} main.cpp)
  target_compile_definitions(${fuzzer}
    PRIVATE
      -DPARSER=${parser}
      -DGETDECODER=${boolvar}
  )

  set_target_properties(${fuzzer} PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(${fuzzer} rawspeed_static)
  target_link_libraries(${fuzzer} libFuzzer)
  target_include_directories(${fuzzer} PUBLIC "${CONFIG_INCLUDE_PATH}")

  if(TARGET libFuzzer_dummy)
    add_test(NAME fuzzers/${fuzzer} COMMAND ${fuzzer})
  endif()

  add_dependencies(ParserFuzzers ${fuzzer})
endfunction()

set(PARSERS
  "Ciff"
  "Fiff"
  "Raw"
  "Tiff"
  "X3f"
)

foreach(parser IN LISTS PARSERS)
  set(parser "${parser}Parser")
  add_parser_fuzzer(${parser} "" false)
  add_parser_fuzzer(${parser} "-GetDecoder" true)
endforeach()
