AvsSource executes an AVS script in a seperate process and proxies audio/video/misc data communication. This enables to workaround the memory limits of 32bit processes by mapping source material to the memory of slaves processes and using the main script for editing and processing.

Plugin functions:
* AvsSource(string avs_source_code)
* AvsImportSource(string avs_file_path)

Examples:
* AvsSource(""" BlankClip() """)
* AvsImportSource("script.avs")

Dependencies:
* Boost 1.57
* Boost.Process 0.5

Runtime:
* AviSynth 2.6
* AviSynth+
