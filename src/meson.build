project('player', 'cpp')
glibdep = dependency('glib-2.0')
pqxxdep = dependency('libpqxx')
cryptodep = dependency('libcrypto')
#xml_rpc_dep = dependency('xmlrpc')
executable('player',
           'main.cpp',
           'music_history.cpp',
           'player.cpp',
           'player_get_next_item.cpp',
           'player_maintenance.cpp',
           'player_playback_transition.cpp',
           'player_run_data.cpp',
           'player_util.cpp',
           'programming_element.cpp',
           'segment.cpp',
           'common/char_array_maths.cpp',
           'common/config_file.cpp',
           'common/exception.cpp',
           'common/dir_list.cpp',
           'common/file.cpp',
           'common/linein.cpp',
           'common/logging.cpp',
           'common/maths.cpp',
           'common/my_string.cpp',
           'common/my_time.cpp',
           'common/mp3_tags.cpp',
           'common/psql.cpp',
           'common/rr_date.cpp',
           'common/rr_misc.cpp',
           'common/rr_misc_db.cpp',
           'common/rr_security.cpp',
           'common/string_splitter.cpp',
           'common/system.cpp',
           'common/temp_dir.cpp',
           'common/xmms_controller.cpp',
           'common/fake_xmmsctrl.cpp',
#           dependencies : [glibdep, pqxxdep, cryptodep, xml_rpc_dep],
           dependencies : [glibdep, pqxxdep, cryptodep],
           #cpp_args: ['-Wall', '-Wextra', '-std=c++14', '-Wshadow', '-Weffc++'])
           cpp_args: ['-Wall', '-Wextra', '-std=c++14'],
#           link_args: ['-lxmlrpc', '-llibxmlrpc_parse', '-llibxmlrpc_xmltok', '-llibxmlrpc_util']
           link_args: ['-L/usr/lib/x86_64-linux-gnu',
                       '-lxmlrpc_client++',
                       '-lxmlrpc_client',
                       '-lxmlrpc++',
                       '-lxmlrpc',
                       '-lxmlrpc_xmlparse',
                       '-lxmlrpc_xmltok',
                       '-lxmlrpc_util',
                       '-L/usr/lib/x86_64-linux-gnu',
#                       '-lcurl',
                       '-lxmlrpc_packetsocket']

#-L/usr/lib/x86_64-linux-gnu   -lxmlrpc_client++ -lxmlrpc_client -lxmlrpc++ -lxmlrpc  -lxmlrpc_xmlparse -lxmlrpc_xmltok -lxmlrpc_util -L/usr/lib/x86_64-linux-gnu -lcurl -lxmlrpc_packetsocket


           )
           # See more over here: http://stackoverflow.com/questions/5283894/recommended-w-flags-for-building-c-with-gcc

