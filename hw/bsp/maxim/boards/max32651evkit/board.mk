MAX_DEVICE = max32650

# Use the secure linker file
LD_FILE = $(FAMILY_PATH)/linker/max32651.ld

# Let the family script know the build needs to be signed
SIGNED_BUILD := 1
