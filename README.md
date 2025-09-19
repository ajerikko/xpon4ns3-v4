# xpon4ns3-v4

VERSION 4.0 
RELEASE DATE 19.09.2025
RELEASED BY DR. JEROME A AROKKIAM (jerom2005raj@gmail.com)

This XGS-PON module contains significant changes to v3.0 (of the XG-PON module for ns-3) published a few years ago. The main changes are given in CHANGELOG (this may not be the exhaustive list, give my limited time in publishing this version of the code; if I get time in the future, I'll look into writing a detained readme; or you may reach me via email and I'll be happy to give guidance and pointers for you to get started with the module)

This version of the code is designed in a way that both XGPON and XGSPON can be run by changing a single parameter from the example script itself (with the helper modules and the changes to the model functions taking care of the necessary changes). 

This module is tested with ns3.41, here's a step-by-step guide to get started with the privded example script:
1. Download and build the base ns-3.41 source code from here: https://www.nsnam.org/releases/ns-3-41/ (once downloaded and extracted, move into the ns-3.41 base folder, then configure with './ns3 configure', then build with './ns3 build')
2. Download and copy the xgsponV4 code to the ./src/ folder inside ns-3.41 base folder and rename the top level folder as 'xgpon' in the ./src/ folder (this is because, in the CMakeLists.txt, the parameter 'LIBNAME' is 'xgpon'); if in doubt, the structure of the 'xgpon' folder would look like any other modules in the ./src/ folder in ns-3)
3. Copy the modified version of ipv4-l3-protcol.cc file in the folder 'changesOnOtherModules' to it's source folder in ns3-41 (cp <xgpon_base_folder>/changesOnOtherModules/src/internet/model/ipv4-l3-protocol_xpon.cc <ns-3.41_base_folder>/src/internet/model/ipv4-l3-protcol.cc)
4. Configure and build the new code in the xgpon folder from the ns-3.41 base folder (configure with './ns3 confgiure', then build with './ns3 build')
5. Copy the example script in the '<xpon base Folder>/src/xgpon/example/' folder to the scratch folder (<ns3.41 base folder>/scratch/) and run the script (./ns3 run scratch/<example.cc>). 
6. Some parameters can be modified from the terminal, but feel free to dive into the example script to make changes as needed. 

Please remember to cite the following journal in publishing your work:
Arokkiam JA, Alvarez P, Wu X, et al. Design, implementation, and evaluation of an XG-PON module for the ns-3 network simulator. SIMULATION. 2017;93(5):409-426. doi:10.1177/0037549716682093 (You can download a pre-print version of it from the university page at: https://cora.ucc.ie/items/bb0363cb-b88c-41bc-9392-c4047577c97b)

You can always reach me on my email if you are stuck or want more guidance in exploring this xg(s)pon module. I maybe slow in responding, as this is not part of my day job. Good luck !!!
  

-Jerome
19.09.2025
