# Starts the editor as an agent starts it, gives it the messages of a file on its standard input,
# and checks what it writes to its standard output.
#
#   cmake -DEDITOR=<WhiteWolfEditor executable> -DMESSAGES=<file of messages> -P RunAgentSession.cmake
#
# The messages are those of Tests/Agent/AgentSession.jsonl: a handshake, the list of tools, an
# entity that is created, looked at and undone, a command that fails, and a request of the
# protocol revision that has no handshake.

cmake_minimum_required(VERSION 3.25)

if(NOT EDITOR OR NOT MESSAGES)
    message(FATAL_ERROR "Pass -DEDITOR=<executable> and -DMESSAGES=<file>.")
endif()

execute_process(
    COMMAND "${EDITOR}" --mcp
    INPUT_FILE "${MESSAGES}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    RESULT_VARIABLE result
    TIMEOUT 120)

# Shown in the test log, and searched by CTest for a report of leaked memory.
message(STATUS "Standard output of the editor:\n${output}")
message(STATUS "Standard error of the editor:\n${errors}")

if(NOT result EQUAL 0)
    message(FATAL_ERROR "The editor ended with '${result}' instead of 0.")
endif()

# One answer to a line for each of the seven requests; the notification is not answered.
string(REGEX MATCHALL "\n" line_ends "${output}")
list(LENGTH line_ends line_count)
if(NOT line_count EQUAL 7)
    message(FATAL_ERROR "The editor wrote ${line_count} lines instead of 7.")
endif()

# Fails the test unless the editor's output contains the given text. A function, because a CMake
# list would not keep texts with square brackets apart.
function(expect_output expected)
    string(FIND "${output}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "The editor's output does not contain: ${expected}")
    endif()
endfunction()

expect_output([=["id":1,"result":{"protocolVersion":"2025-11-25"]=])
expect_output([=["serverInfo":{"name":"WhiteWolfEditor"]=])
expect_output([=["id":2,"result":{"tools":]=])
expect_output([=["name":"Entity.Create"]=])
expect_output([=["id":3,"result":{"content":[{"type":"text","text":"{\n  \"Id\": 1\n}"}],"isError":false}]=])
expect_output([=[\"Name\": \"Player\"]=])
expect_output([=[\"Undone\": \"Entity.Create\"]=])
expect_output([=["id":6,"result":{"content":[{"type":"text","text":"There is no entity with the identifier 99."}],"isError":true}]=])
expect_output([=["id":"discover-1","result":{"supportedVersions":["2026-07-28"]]=])
expect_output([=["resultType":"complete"]=])
message(STATUS "The editor served the session as expected.")
