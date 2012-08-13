EnsureSConsVersion(0,96)
EnsurePythonVersion(2,3)
SConsignFile()

import os
import re
import tarfile

PACKAGE = 'MotionTrackOSC'
VERSION = '0.2a'

################ FUNCTIONS ##################
def dist_paths():
    paths = DIST_FILES[:]
    specs = DIST_SPECS[:]
    while specs:
        base, re = specs.pop()
        if not re: re = ANY_FILE_RE
        for root, dirs, files in os.walk(base):
            if 'CVS' in dirs: dirs.remove('CVS')
            if '.svn' in dirs: dirs.remove('.svn')
            for path in dirs[:]:
                if re.match(path):
                    specs.append((os.path.join(root, path), re))
                    dirs.remove(path)
            for path in files:
                if re.match(path):
                    paths.append(os.path.join(root, path))
    paths.sort()
    return paths

def build_tar(env, target, source):
    paths = dist_paths()
    tarfile_name = str(target[0])
    tar_name = os.path.splitext(os.path.basename(tarfile_name))[0]
    tar = tarfile.open(tarfile_name, "w:bz2")
    for path in paths:
        tar.add(path, os.path.join(tar_name, path))
    tar.close()


def createEnvironment(*keys):
    env = os.environ
    res = {}
    for key in keys:
        if env.has_key(key):
            res[key] = env[key]
    return res

def CheckPKGConfig( context, version ):
    context.Message( 'pkg-config installed? ' )
    ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
    context.Result( ret )
    return ret

def LookForPackage( context, name ):
    context.Message('%s installed? ' % name)
    ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
    context.Result( ret )
    return ret

print "\n=============== " + PACKAGE + ' v' + VERSION + " ================="
print "=========== (c) 2009 - Marije Baalman ============== "
print "=========== www.nescivi.nl/motiontrackosc ==========\n"
# Read options from the commandline
opts = Options(None, ARGUMENTS)
opts.AddVariables(
    PathVariable('PREFIX', 'Set the installation directory', '/usr/local'),
    BoolVariable('SC', 'Install SC class', False),
    PathVariable('SCPATH', 'Set the supercollider directory to install to', os.environ.get('HOME')+'/share/SuperCollider/Extensions/' )
)

env = Environment(options = opts,
                  ENV     = createEnvironment('PATH', 'PKG_CONFIG_PATH'),
                  PACKAGE = PACKAGE,
                  VERSION = VERSION,
                  URL     = 'http://www.nescivi.nl/motiontrackosc',
                  TARBALL = PACKAGE + '.' + VERSION + '.tbz2'
		)

# Help
Help("""
Command Line options:
scons -h         (MotionTrackOSC help) 
scons -H         (SCons help) 
      """)

Help( opts.GenerateHelpText( env ) )


# ensure that pkg-config and external dependencies are ok
# and parse them
conf = env.Configure(custom_tests = {  'CheckPKGConfig' : CheckPKGConfig,
                                       'LookForPackage' : LookForPackage })

if not conf.CheckPKGConfig('0'):
    print 'pkg-config not found.'
    Exit(1)

# liblo is needed
if not conf.LookForPackage('liblo'):
    Exit(1)
env.ParseConfig('pkg-config --cflags --libs liblo')

# opencv is needed
if not conf.LookForPackage('opencv'):
    Exit(1)
env.ParseConfig('pkg-config --cflags --libs opencv')

env = conf.Finish()

prog = env.Program('motiontrackosc', 'motiontrackosc.c')

BIN_DIR = env['PREFIX'] + "/bin"

env.Alias( 'install', env.Install(BIN_DIR, prog) )

SC_FILES = Split('''
	supercollider/classes/MotionTrackOSC.sc
	supercollider/classes/MotionTracker.sc
	supercollider/classes/LowNumberAllocator.sc
	supercollider/classes/extPoint.sc
''')

SC_HELP_FILES = Split('''
	supercollider/MotionTrackOSC.help.scd
	supercollider/MotionTracker.help.scd
''')

if env['SC']:
  SCPATH = env['SCPATH'] + 'MotionTrackOSC/'

env.Alias( 'SC', env.Install( env['SCPATH'], SC_FILES ) )
env.Alias( 'SC', env.Install( env['SCPATH'] + 'Help/', SC_HELP_FILES ) )

DIST_FILES = Split('''
README
COPYING
SConstruct
motiontrackosc.c
''')

ANY_FILE_RE = re.compile('.*')

DIST_SPECS = [
    ('supercollider', ANY_FILE_RE ),
    ]

if 'dist' in COMMAND_LINE_TARGETS:
    env.Alias('dist', env['TARBALL'])
    env.Command(env['TARBALL'], 'SConstruct', build_tar)
