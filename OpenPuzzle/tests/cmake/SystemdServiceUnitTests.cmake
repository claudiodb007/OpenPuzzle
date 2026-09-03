if(NOT DEFINED OPENPUZZLE_SERVICE_FILE)
    message(FATAL_ERROR
        "OPENPUZZLE_SERVICE_FILE is required"
    )
endif()

if(NOT EXISTS "${OPENPUZZLE_SERVICE_FILE}")
    message(FATAL_ERROR
        "OpenPuzzle systemd service file is missing"
    )
endif()

file(
    READ
    "${OPENPUZZLE_SERVICE_FILE}"
    OPENPUZZLE_SERVICE_CONTENT
)

set(OPENPUZZLE_REQUIRED_SERVICE_LINES
    "ExecStart=/usr/bin/openpuzzle run $OPENPUZZLE_SERVICE_ARGS --backend %i"
    "Environment=OPENPUZZLE_EXECUTION_SLOT=primary"
    "Environment=OPENPUZZLE_SERVICE_ARGS="
    "EnvironmentFile=-%h/.config/OpenPuzzle/%i.env"
    "Restart=on-failure"
    "TimeoutStopSec=180s"
    "KillMode=mixed"
    "KillSignal=SIGTERM"
    "UMask=0077"
    "WantedBy=default.target"
)

foreach(OPENPUZZLE_REQUIRED_LINE
        IN LISTS
        OPENPUZZLE_REQUIRED_SERVICE_LINES)
    string(
        FIND
        "${OPENPUZZLE_SERVICE_CONTENT}"
        "${OPENPUZZLE_REQUIRED_LINE}"
        OPENPUZZLE_REQUIRED_LINE_POSITION
    )

    if(OPENPUZZLE_REQUIRED_LINE_POSITION EQUAL -1)
        message(FATAL_ERROR
            "Missing systemd service line: "
            "${OPENPUZZLE_REQUIRED_LINE}"
        )
    endif()
endforeach()

set(OPENPUZZLE_FORBIDDEN_SERVICE_LINES
    "ExecStart=/usr/bin/openpuzzle run --backend %i $OPENPUZZLE_SERVICE_ARGS"
    "ExecStop=/usr/bin/openpuzzle stop"
)

foreach(OPENPUZZLE_FORBIDDEN_LINE
        IN LISTS
        OPENPUZZLE_FORBIDDEN_SERVICE_LINES)
    string(
        FIND
        "${OPENPUZZLE_SERVICE_CONTENT}"
        "${OPENPUZZLE_FORBIDDEN_LINE}"
        OPENPUZZLE_FORBIDDEN_LINE_POSITION
    )

    if(NOT OPENPUZZLE_FORBIDDEN_LINE_POSITION EQUAL -1)
        message(FATAL_ERROR
            "Unsafe systemd service line: "
            "${OPENPUZZLE_FORBIDDEN_LINE}"
        )
    endif()
endforeach()

message(STATUS
    "OpenPuzzle systemd service unit verified"
)
