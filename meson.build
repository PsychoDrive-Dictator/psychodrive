project(
  'psychodrive',
  ['c','cpp'],
  version : run_command('git', 'describe').stdout().strip(),
  default_options : ['warning_level=3','cpp_std=c++20']
)

project_source_files = [
  'main.cpp',
  'simulation.cpp',
  'chara.cpp',
  'guy.cpp',
  'ui.cpp',
  'combogen.cpp',
  'input.cpp',
  'render.cpp',
  'imgui/imgui.cpp',
  'imgui/imgui_draw.cpp',
  'imgui/imgui_widgets.cpp',
  'imgui/imgui_tables.cpp',
  'imgui/imgui_demo.cpp',
  'imgui_neo_sequencer.cpp',
  'imgui_neo_internal.cpp',
  'implot.cpp',
  'implot_demo.cpp',
  'implot_items.cpp',
  'imgui/backends/imgui_impl_sdl2.cpp',
  'imgui/backends/imgui_impl_opengl3.cpp',
]

project_dependencies = [
  dependency('SDL2'),
  dependency('GL'),
]

build_args = [
]

include_dirs = [ 
  include_directories('imgui'),
  include_directories('imgui/backends'),
]

bindist_dir = meson.current_build_dir() + '/' + meson.project_name() + '-' + meson.project_version()

compiler = meson.get_compiler('c', native : false)
compiler_id = compiler.get_id()
target_os = host_machine.system()

if compiler_id == 'gcc' and target_os == 'windows'
  message('MinGW path')
  add_global_arguments('-static-libgcc', language : 'cpp')
  add_global_arguments('-static-libstdc++', language : 'cpp')
  add_global_arguments('-Wl,-Bstatic', language : 'cpp')
  add_global_arguments('-lstdc++', language : 'cpp')
  add_global_link_arguments('-static-libgcc', language : 'cpp')
  add_global_link_arguments('-static-libstdc++', language : 'cpp')
  add_global_link_arguments('-Wl,-Bstatic', language : 'cpp')
  add_global_link_arguments('-lstdc++', language : 'cpp')
  add_global_link_arguments('-lpthread', language : 'cpp')

  install_data('SDL2.dll', install_dir: bindist_dir)
endif

if compiler_id == 'emscripten'
  message('emscripten path')
  add_global_arguments('-pthread', language : 'cpp')
  add_global_arguments('-Wno-nontrivial-memaccess', language : 'cpp')
  add_global_arguments('-Wno-dollar-in-identifier-extension', language : 'cpp')
  add_global_link_arguments('-pthread', language : 'cpp')
  add_global_link_arguments('-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency', language : 'cpp')
  add_global_link_arguments('-sMALLOC=mimalloc', language : 'cpp')
  add_global_link_arguments('-sMAX_WEBGL_VERSION=2', language : 'cpp')
  add_global_link_arguments('-sMAX_WEBGL_VERSION=2', language : 'cpp')
  add_global_link_arguments('-sFORCE_FILESYSTEM', language : 'cpp')
  add_global_link_arguments('-sALLOW_MEMORY_GROWTH', language : 'cpp')
  add_global_link_arguments('-sNO_DISABLE_EXCEPTION_CATCHING', language : 'cpp')
  add_global_link_arguments('-sEXPORTED_FUNCTIONS=_main,_jsLog,_jsCharLoadCallback,_jsModalDropDownSelection', language : 'cpp')
  add_global_link_arguments('-sEXPORTED_RUNTIME_METHODS=ccall,cwrap', language : 'cpp')

  install_data('psychodrive.html', install_dir: bindist_dir)
else
  project_source_files += 'gl3w.c'
  project_dependencies += dependency('jemalloc', static : true)
endif

# ===================================================================

# ======
# Target
# ======

build_args += [
  '-DPROJECT_NAME=' + meson.project_name(),
  '-DPROJECT_VERSION=' + meson.project_version(),
]

project_target = executable(
  meson.project_name(),
  project_source_files,
  dependencies: project_dependencies,
  include_directories : include_dirs,
  c_args : build_args,
  cpp_args : build_args,
  install : true,
  install_dir: bindist_dir,
)

message(project_target.full_path())

meson.add_install_script('meson_package_data.sh', bindist_dir, compiler_id)
