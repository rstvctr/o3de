{ 
    "Source" : "./MultiviewForwardPassSrg.azsl",

    "Definitions" : ["MULTIVIEW=1"],

    "DepthStencilState" : 
    {
        "Depth" : 
        { 
            "Enable" : false 
        },
        "Stencil" :
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
            "Name": "",
            "RemoveBuildArguments": {
                "azslc": ["--strip-unused-srgs"]
            }
        }
    ]
}
