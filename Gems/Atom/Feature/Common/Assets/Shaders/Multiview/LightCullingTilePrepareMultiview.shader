{
    "Source": "../LightCulling/LightCullingTilePrepare.azsl",

    "Definitions" : ["MULTIVIEW=1"],
    
    "ProgramSettings" : 
    {
        "EntryPoints":
        [
            {
                "name":  "MainCS",
                "type" : "Compute"
            }
        ] 
    },

    "Supervariants":
    [
        {
            "Name": "NoMSAA",
            "AddBuildArguments": {
                "azslc": ["--no-ms"]
            }
        }
    ]
}
