# Registering an Nsight Systems Plugin

## Create an Nsight Systems Plugin

To create an Nsight Systems plugin follow the instructions listed in the [Nsight Systems user guide](https://docs.nvidia.com/nsight-systems/UserGuide/index.html#nsight-systems-plugins).

If you wish to publish your plugin in this site, follow the next step:

## Register an Nsight Systems Plugin

To register an Nsight Systems plugin in this GitHub repository, follow these steps:

1. Clone the repository
2. Create a local branch
3. Create a json file describing the plugin and place it under the "PluginFiles" directory
    - The "SiteURL" variable should contain the URL from which users can download the plugin
    - The "Architectures" variable describes the plugin's supported architectures. Available values are "x64" and "aarch64".
    - The "OperatingSystems" variable describes the plugin's supported operating systems. Available values are "Windows" and "Linux".
    - If your plugin requires a special Nsight Systems version, please use the MinNsightSystemsVersion and MaxNsightSystemsVersion to specify that.
4. Optionally (but recommended), place a plugin screen shot under the "PluginFiles/Images" directory. The screen shot should be pointed to by the json file's "Images" array.
5. Push a merge request of your branch to be reviewed by the Nsight Systems team.

## Tips

Use the "scripts/run_build_worklow_locally.sh" script to generate the plugins list locally. This enables viewing of the resulting list before pushing the merge request. After running the script, the plugins list will appear under the "Pages" directory. Load the "Pages/index.html" file into a browser to view it.
