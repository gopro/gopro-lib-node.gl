project('ngl-tests', meson_version: '>= 0.56.0')

ngl_test = find_program('ngl-test')
ngl_probe = find_program('ngl-probe')

backends = []
probe_output = run_command(ngl_probe).stdout()
message('ngl-probe:\n' + probe_output)
foreach line : probe_output.split('\n')
  if line.startswith('- ')
    backends += line.substring(2, -1)
  endif
endforeach

media_test_filename = 'ngl-media-test.nut'
media_test_file = custom_target(
  media_test_filename,
  command: [
    find_program('ffmpeg'),
    '-nostdin', '-nostats',
    '-f', 'lavfi', '-i', 'testsrc=d=30:r=60',
    '-c:v', 'ffv1',
    '-y', '@OUTPUT@',
  ],
  output: media_test_filename,
)

foreach backend : backends
  cap_cmd = [ngl_probe, '-l', 'quiet', '-b', backend, '--cap']
  # ngl-probe may display garbage on stdout: typically Mesa does this on
  # offscreen at initialization, this is why we have .split()[-1] to read the
  # last word in the output.
  has_compute = run_command(cap_cmd + ['compute']).stdout().split()[-1].to_int() == 1 ? true : false
  max_samples = run_command(cap_cmd + ['max_samples']).stdout().split()[-1].to_int()

  message('Backend: ' + backend)
  message('- Compute: @0@'.format(has_compute))
  message('- Max samples: @0@'.format(max_samples))

  tests_anim = [
    'forward_api',
    'forward_float',
    'forward_vec2',
    'forward_vec3',
    'forward_vec4',
    'forward_quat',
    'resolution_api',
  ]

  tests_api = [
    'backend',
    'reconfigure',
    'reconfigure_clearcolor',
    'reconfigure_fail',
    'ctx_ownership',
    'ctx_ownership_subgraph',
    'capture_buffer_lifetime',
    'hud',
    'text_live_change',
    'media_sharing_failure',
  ]

  tests_blending = [
    'all_diamond',
    'all_timed_diamond',
    'none',
    'multiply',
    'screen',
    'darken',
    'lighten',
  ]

  tests_compute = []
  if has_compute
    tests_compute += [
      'animation',
      'histogram',
      'particules',
    ]
  endif

  uniform_names = [
    'single_bool',
    'single_float',
    'single_int',
    'single_ivec2',
    'single_ivec3',
    'single_ivec4',
    'single_uint',
    'single_uvec2',
    'single_uvec3',
    'single_uvec4',
    'single_mat4',
    'single_quat_mat4',
    'single_quat_vec4',
    'single_vec2',
    'single_vec3',
    'single_vec4',
    'animated_quat_mat4',
    'animated_quat_vec4',
    'array_float',
    'array_vec2',
    'array_vec3',
    'array_vec4',
  ]

  block_names = uniform_names + [
    'animated_buffer_float',
    'animated_buffer_vec2',
    'animated_buffer_vec3',
    'animated_buffer_vec4',
    'array_int',
    'array_ivec2',
    'array_ivec3',
    'array_ivec4',
    'array_mat4',
  ]

  tests_data = [
    'streamed_buffer_vec4',
    'streamed_buffer_vec4_time_anim',
  ]
  foreach test_name : uniform_names
    tests_data += test_name + '_uniform'
  endforeach
  foreach test_name : block_names
    tests_data += test_name + '_std140'
  endforeach
  if has_compute
    foreach test_name : block_names
      tests_data += test_name + '_std430'
    endforeach
  endif

  live_names = [
    'single_bool',
    'single_float',
    'single_int',
    'single_mat4',
    'single_quat_mat4',
    'single_quat_vec4',
    'single_vec2',
    'single_vec3',
    'single_vec4',
    'trf_single_mat4',
  ]

  tests_live = []
  foreach test_name : live_names
    tests_live += test_name + '_uniform'
  endforeach
  foreach test_name : live_names
    tests_live += test_name + '_std140'
  endforeach
  if has_compute
    foreach test_name : live_names
      tests_live += test_name + '_std430'
    endforeach
  endif

  tests_media = [
    'phases_display',
    'phases_resources',
  ]

  tests_rtt = [
    'load_attachment',
    'feature_depth',
    'feature_depth_stencil',
    'mipmap',
    'sample_depth',
    'texture_depth',
    'texture_depth_stencil',
    'clear_attachment_with_timeranges',
  ]

  if max_samples >= 4
    tests_rtt += [
      'feature_depth_msaa',
      'feature_depth_stencil_msaa',
      'texture_depth_msaa',
      'texture_depth_stencil_msaa',
    ]
  endif

  tests_shape = [
    'precision_iovar',
    'triangle',
    'triangle_cull_back',
    'triangle_cull_front',
    'triangles_mat4_attribute',
    'quad',
    'quad_cull_back',
    'quad_cull_front',
    'circle',
    'circle_cull_back',
    'circle_cull_front',
    'diamond_colormask',
    'geometry',
    'geometry_normals',
    'geometry_indices',
    'geometry_normals_indices',
    'morphing',
    'cropboard',
    'cropboard_indices',
  ]

  if max_samples >= 4
    tests_shape += [
      'triangle_msaa',
    ]
  endif

  tests_text = [
    'colors',
    '0_to_127',
    'align_cc',
    'align_cr',
    'align_cl',
    'align_bc',
    'align_br',
    'align_bl',
    'align_tc',
    'align_tr',
    'align_tl',
  ]

  tests_texture = [
    '3d',
    'clear_and_scissor',
    'cubemap',
    'cubemap_from_mrt',
    'data',
    'data_animated',
    'data_unaligned_row',
    'mipmap',
    'scissor',
  ]

  if max_samples >= 4
    tests_texture += [
      'cubemap_from_mrt_msaa',
    ]
  endif

  tests_transform = [
    'animated_camera',
    'matrix',
    'translate',
    'translate_animated',
    'scale',
    'scale_animated',
    'scale_anchor',
    'scale_anchor_animated',
    'rotate',
    'rotate_anchor',
    'rotate_quat',
    'rotate_quat_anchor',
    'rotate_quat_animated',
  ]

  tests = {
    'api':       {'tests': tests_api, 'has_refs': false},
    'anim':      {'tests': tests_anim},
    'blending':  {'tests': tests_blending},
    'compute':   {'tests': tests_compute},
    'data':      {'tests': tests_data},
    'live':      {'tests': tests_live},
    'media':     {'tests': tests_media, 'depends': media_test_file},
    'rtt':       {'tests': tests_rtt},
    'shape':     {'tests': tests_shape},
    'text':      {'tests': tests_text},
    'texture':   {'tests': tests_texture},
    'transform': {'tests': tests_transform},
  }

  env = ['BACKEND=' + backend]

  foreach category, test_specs : tests
    test_names = test_specs.get('tests')
    has_refs = test_specs.get('has_refs', true)
    depends = test_specs.get('depends', [])

    foreach test_name : test_names
      func_name = '@0@_@1@'.format(category, test_name)

      args = [files(category + '.py'), func_name]
      if has_refs
        # We don't use files() here because the file may not exist yet (in case
        # we just added the test and want to generate it)
        args += meson.current_source_dir() / 'refs/@0@.ref'.format(func_name)
      endif

      test(
        test_name,
        ngl_test,
        args: args,
        env: env,
        depends: depends,
        suite: [backend, category],
      )
    endforeach
  endforeach
endforeach
