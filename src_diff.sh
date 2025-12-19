
# Add upstream once
git remote add upstream https://github.com/gridlab-d/gridlab-d.git

# Fetch the branch you want to compare against
git fetch upstream feature/1478

# Produce unified diffs for the target set
git --no-pager diff -U3 upstream/feature/1478 -- \
  gldcore/object.cpp gldcore/object.h gldcore/exec.cpp gldcore/module.cpp gldcore/deltamode.cpp \
  powerflow/node.cpp powerflow/meter.cpp powerflow/substation.cpp powerflow/solver_nr.cpp powerflow/powerflow.cpp \
  generators/diesel_dg.cpp \
  > mac-arm64-deferred-init.diff

# (Optional) compress the patch
#gzip -9 mac-arm64-deferred-init.diff


