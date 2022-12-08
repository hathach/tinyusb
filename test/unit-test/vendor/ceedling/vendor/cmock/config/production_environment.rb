# ==========================================
#   CMock Project - Automatic Mock Generation for C
#   Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
#   [Released under MIT License. Please refer to license.txt for details]
# ==========================================

# Setup our load path:
[
  'lib'
].each do |dir|
  $:.unshift(File.join(__dir__ + '/../', dir))
end
