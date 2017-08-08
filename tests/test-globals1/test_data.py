# RMW both a static and a non-static global
# Also tests proper .data copying

test_data = {
    "desc": "RMW static and non-static globals",
    "modules": [["mod_globals1.c"]],
    "required": ["Running test 'mod_globals1'"]
}
