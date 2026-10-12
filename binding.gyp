{
  "targets": [
    {
      "target_name": "flowjs_native",
      "sources": [
        "src_c/arena.c",
        "src_c/buf.c",
        "src_c/lexer.c",
        "src_c/parser.c",
        "src_c/ast.c",
        "src_c/transform.c",
        "src_c/emitter.c",
        "src_c/addon.c"
      ],
      "include_dirs": [
        "src_c"
      ],
      "defines": [
        "NAPI_VERSION=8"
      ],
      "conditions": [
        [
          "OS=='linux'",
          {
            "cflags": [
              "-std=c2x",
              "-Wall",
              "-Wextra",
              "-Wno-unused-parameter",
              "-fvisibility=hidden",
              "-O2"
            ]
          }
        ],
        [
          "OS=='mac'",
          {
            "xcode_settings": {
              "OTHER_CFLAGS": [
                "-std=c2x",
                "-Wall",
                "-Wextra",
                "-Wno-unused-parameter",
                "-fvisibility=hidden",
                "-O2"
              ]
            }
          }
        ],
        [
          "OS=='win'",
          {
            "msvs_settings": {
              "VCCLCompilerTool": {
                "AdditionalOptions": [
                  "/std:c11"
                ]
              }
            }
          }
        ]
      ]
    }
  ]
}