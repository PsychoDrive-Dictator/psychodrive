project(
  'psychodrive',
  ['c','cpp'],
  version : '0.0.1',
  default_options : ['warning_level=3']
)

project_source_files = [
  'main.cpp',
  'guy.cpp',
  'ui.cpp',
  'input.cpp',
  'render.cpp',
  'gl3w.c',
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

if meson.is_cross_build()
  add_global_arguments('-static-libgcc', language : 'cpp')
  add_global_arguments('-static-libstdc++', language : 'cpp')
  add_global_arguments('-Wl,-Bstatic', language : 'cpp')
  add_global_arguments('-lstdc++', language : 'cpp')
  add_global_link_arguments('-static-libgcc', language : 'cpp')
  add_global_link_arguments('-static-libstdc++', language : 'cpp')
  add_global_link_arguments('-Wl,-Bstatic', language : 'cpp')
  add_global_link_arguments('-lstdc++', language : 'cpp')
  add_global_link_arguments('-lpthread', language : 'cpp')
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
  install : true,
  c_args : build_args,
)
