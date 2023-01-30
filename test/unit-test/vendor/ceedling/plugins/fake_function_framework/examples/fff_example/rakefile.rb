# This change from the default is for running Ceedling out of another folder.
PROJECT_CEEDLING_ROOT = "../../../.."
load "#{PROJECT_CEEDLING_ROOT}/lib/ceedling.rb"

Ceedling.load_project

task :default => %w[ test:all release ]
