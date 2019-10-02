project('steamcompmgr', ['c','cpp'])

dep_x11 = dependency('x11')
dep_xdamage = dependency('xdamage')
dep_xcomposite = dependency('xcomposite')
dep_xrender = dependency('xrender')
dep_xext = dependency('xext')
dep_gl = dependency('GL')
dep_xxf86vm = dependency('xxf86vm')

executable(
    'steamcompmgr',
    'src/steamcompmgr.c',
    dependencies : [
        dep_x11, dep_xdamage, dep_xcomposite, dep_xrender, dep_xext, dep_gl,
        dep_xxf86vm
    ],
)
