namespace eval kiwi {

    ## Generates txt files for each used ipcore in the directory "./ipcore_properties"
    ## After ip has been reconfigured in the Vivado UI, run script by typing in Vivado tcl console:
    ##      cd <whatever dir needed>/import_srcs
    ##      source kiwi.tcl
    ##      kiwi::update_ip_properties
    proc update_ip_properties {} {
        foreach p [get_ips] {
            report_property -quiet -regexp -file [file normalize "./ipcore_properties/${p}.txt"] [get_ips $p] "(CONFIG.*|IPDEF)"
        }
    }


    ## makes ipcores with properties taken from txt files in the directory "./ipcore_properties"
    proc make_ipcores {} {
        proc generate_ipcore ps {
            set targetDir {generated}
            set module_name [dict get $ps "CONFIG.Component_Name"]
            set ps [dict remove $ps CONFIG.Component_Name]

            set module_xci "${targetDir}/${module_name}/${module_name}.xci"
            set ipdef [split [dict get $ps "IPDEF"] ":"]
            create_ip \
                -vendor  [lindex $ipdef 0] \
                -library [lindex $ipdef 1] \
                -name    [lindex $ipdef 2] \
                -module_name ${module_name} \
                -dir ${targetDir}
            set ps [dict remove $ps IPDEF]

            set_property -dict $ps [get_ips ${module_name}]

            generate_target {instantiation_template} [get_files ${module_xci}]
            generate_target all [get_files  ${module_xci}]
            catch {
                config_ip_cache -export [get_ips -all ${module_name}]
            }
            export_ip_user_files -of_objects [get_files ${module_xci}] -no_script -sync -force -quiet
        }

        proc ipcore_property_file_to_dict fn {
            set fp [open $fn]
            set headers {Property Type Read-only Value}
            array set idx {}
            foreach h $headers {
                set idx($h) -1
            }
            set ps [dict create]
            while {-1 != [gets $fp line]} {
                ##puts "The current line is '$line'."
                if {$idx(Property) == -1} {
                    foreach h $headers {
                        set idx($h) [string first $h $line]
                    }
                } else {
                    set key [string trim [string range $line $idx(Property) \
                                              [expr $idx(Type) - 1]]]
                    set val [string trim [string range $line $idx(Value) \
                                              [expr [string length $line] - 1]]]
                    dict set ps $key $val
                }
            }
            close $fp
            return $ps
        }

        foreach f [glob ipcore_properties/ipcore_*.txt] {
            generate_ipcore [ipcore_property_file_to_dict $f]
        }
    }
}

# # http://grittyengineer.com/creating-vivado-ip-the-smart-tcl-way/

# package require struct::list

# proc gen_dds_sin_cos {output_width phase_width} {

#     set targetDir {generated}
#     set module_name "ipcore_dds_sin_cos_13b_${output_width}b_${phase_width}b_test"
#     set module_xci "${targetDir}/${module_name}/${module_name}.xci"

#     create_ip -name dds_compiler -vendor xilinx.com -library ip -module_name ${module_name} -dir ${targetDir}

#     ## report_property  [get_ips ipcore_dds_sin_cos_13b_15b] CONFIG.*
#     set prop_list [list CONFIG.ACLK_INTF.FREQ_HZ           {100000000} \
#                        CONFIG.Amplitude_Mode               {Full_Range} \
#                        CONFIG.Channels                     {1} \
#                        CONFIG.Component_Name               "${module_name}" \
#                        CONFIG.DATA_Has_TLAST               {Not_Required} \
#                        CONFIG.DDS_Clock_Rate               {66.6666} \
#                        CONFIG.DSP48_Use                    {Minimal} \
#                        CONFIG.Frequency_Resolution         {0.4} \
#                        CONFIG.GUI_Behaviour                {Coregen} \
#                        CONFIG.Has_ACLKEN                   {false} \
#                        CONFIG.Has_ARESETn                  {false} \
#                        CONFIG.Has_Phase_Out                {false} \
#                        CONFIG.Has_TREADY                   {false} \
#                        CONFIG.Latency                      {8} \
#                        CONFIG.Latency_Configuration        {Auto} \
#                        CONFIG.M_DATA_Has_TUSER             {Not_Required} \
#                        CONFIG.M_PHASE_Has_TUSER            {Not_Required} \
#                        CONFIG.Memory_Type                  {Block_ROM} \
#                        CONFIG.Mode_of_Operation            {Standard} \
#                        CONFIG.Modulus                      {9} \
#                        CONFIG.Negative_Cosine              {false} \
#                        CONFIG.Negative_Sine                {false} \
#                        CONFIG.Noise_Shaping                {Phase_Dithering} \
#                        CONFIG.OUTPUT_FORM                  {Twos_Complement} \
#                        CONFIG.Optimization_Goal            {Area} \
#                        CONFIG.Output_Selection             {Sine_and_Cosine} \
#                        CONFIG.Output_Width                 "${output_width}" \
#                        CONFIG.POR_mode                     {false} \
#                        CONFIG.Parameter_Entry              {Hardware_Parameters} \
#                        CONFIG.PartsPresent                 {Phase_Generator_and_SIN_COS_LUT} \
#                        CONFIG.Phase_Increment              {Streaming} \
#                        CONFIG.Phase_Width                  "${phase_width}" \
#                        CONFIG.Phase_offset                 {None} \
#                        CONFIG.Resync                       {false} \
#                        CONFIG.S_CONFIG_Sync_Mode           {On_Vector} \
#                        CONFIG.S_PHASE_Has_TUSER            {Not_Required} \
#                        CONFIG.S_PHASE_TUSER_Width          {1} \
#                        CONFIG.Spurious_Free_Dynamic_Range  {45} \
#                        CONFIG.explicit_period              {false} \
#                        CONFIG.period                       {1} \
#                       ]

#     foreach x [struct::list iota 16] {
#         incr x;
#         lappend prop_list "CONFIG.Output_Frequency${x}"
#         lappend prop_list {0}
#         lappend prop_list "CONFIG.PINC${x}"
#         lappend prop_list {0}
#         lappend prop_list "CONFIG.POFF${x}"
#         lappend prop_list {0}
#         lappend prop_list "CONFIG.Phase_Offset_Angles${x}"
#         lappend prop_list {0}
#     }
#     set_property -dict ${prop_list} [get_ips ${module_name}]

#     generate_target {instantiation_template} [get_files ${module_xci}]
#     generate_target all [get_files  ${module_xci}]
#     catch { config_ip_cache -export [get_ips -all ${module_name}] }
#     export_ip_user_files -of_objects [get_files ${module_xci}] -no_script -sync -force -quiet

# }

# proc gen_acc {width} {

#     set targetDir {generated}
#     set module_name "ipcore_acc_u${width}b_test"
#     set module_xci "${targetDir}/${module_name}/${module_name}.xci"

#     create_ip -name c_accum -vendor xilinx.com -library ip -module_name ${module_name} -dir ${targetDir}

#     ## report_property  [get_ips ipcore_acc_u32b] CONFIG.*
#     set prop_list [list CONFIG.AINIT_Value           {0} \
#                        CONFIG.Accum_Mode             {Add} \
#                        CONFIG.Bypass                 {false} \
#                        CONFIG.Bypass_Sense           {Active_High} \
#                        CONFIG.CE                     {false} \
#                        CONFIG.CLK_INTF.FREQ_HZ       {100000000} \
#                        CONFIG.C_In                   {false} \
#                        CONFIG.Component_Name         "${module_name}" \
#                        CONFIG.Implementation         {DSP48} \
#                        CONFIG.Input_Type             {Unsigned} \
#                        CONFIG.Input_Width            "${width}" \
#                        CONFIG.Latency                {1} \
#                        CONFIG.Latency_Configuration  {Manual} \
#                        CONFIG.Output_Width           "${width}" \
#                        CONFIG.SCLR                   {true} \
#                        CONFIG.SINIT                  {false} \
#                        CONFIG.SINIT_Value            {0} \
#                        CONFIG.SSET                   {false} \
#                        CONFIG.Scale                  {0} \
#                        CONFIG.SyncCtrlPriority       {Reset_Overrides_Set} \
#                        CONFIG.Sync_CE_Priority       {Sync_Overrides_CE}
#                       ]

#     set_property -dict ${prop_list} [get_ips ${module_name}]

#     generate_target {instantiation_template} [get_files ${module_xci}]
#     generate_target all [get_files  ${module_xci}]
#     catch { config_ip_cache -export [get_ips -all ${module_name}] }
#     export_ip_user_files -of_objects [get_files ${module_xci}] -no_script -sync -force -quiet

# }
