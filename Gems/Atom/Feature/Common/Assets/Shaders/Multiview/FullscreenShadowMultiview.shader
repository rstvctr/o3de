{
    "Source" : "../Shadow/FullscreenShadow.azsl",

    "Definitions" : ["MULTIVIEW=1"],

    "RasterState" :
    {
        "CullMode" : "None"
    },

    "DepthStencilState" :
    {
        "Depth" :
        {
            "Enable" : false
        }
    },

    "ProgramSettings":
    {
      "EntryPoints": 
      [
        {
          "name": "MainVS",
          "type": "Vertex"
        },
        {
          "name": "MainPS",
          "type": "Fragment"
        }
      ]
    },

    "Supervariants":
    [
        {
            "Name": "NoMSAA",
                "AddBuildArguments" : {
                "azslc": ["--no-ms"]
            }
        }
    ],

    "MultiviewLayers": 2

    // Todo: test Compute Shader version with async compute and LDS optimizations
    // "ProgramSettings" :
    // {
    //     "EntryPoints":
    //     [
    //         {
    //             "name" : "MainCS",
    //             "type" : "Compute"
    //         }
    //     ]
    // }

}
