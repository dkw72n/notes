meta:
  id: luac
  file-extension: out
  endian: le
doc: https://www.lua.org/source/5.2/lundump.c.html
seq:
  - id: header
    type: lua_header
  - id: n
    type: u1
  - id: function
    type: lua_function
  - id: remain
    size-eos: true

types:
  lua_header:
    seq:
    - id: signature
      contents: [27, 'L', 'u', 'a']
    - id: version
      contents: [0x53]
    - id: format
      type: u1
    - id: lua_tail
      contents: [0x19, 0x93, 0x0d, 0x0a, 0x1a, 0x0a]
    - id: sizeof_int
      type: u1
    - id: sizeof_size_t
      type: u1
    - id: sizeof_instruction
      type: u1
    - id: sizeof_lua_integer
      type: u1
    - id: sizeof_lua_number
      type: u1
    - id: lua_integer_validator
      type: u8
    - id: lua_number_validator
      type: f8
    
  lua_function:
    seq:
    - id: source
      type: lua_string
    - id: line_defined
      type: u4
    - id: last_line_defined
      type: u4
    - id: num_params
      type: u1
    - id: is_vararg
      type: u1
    - id: max_stack_size
      type: u1
    - id: size_code
      type: u4
    - id: code
      type: instruction
      repeat: expr
      repeat-expr: size_code
    - id: size_k
      type: u4
    - id: k
      type: t_value
      repeat: expr
      repeat-expr: size_k
    - id: size_upvalues
      type: u4
    - id: upvalues
      type: upvalue
      repeat: expr
      repeat-expr: size_upvalues
    - id: size_p
      type: u4
    - id: p 
      type: lua_function
      repeat: expr
      repeat-expr: size_p
    - id: size_lineinfo
      type: u4
    - id: lineinfo
      type: u4
      repeat: expr
      repeat-expr: size_lineinfo
    - id: size_locvars
      type: u4
    - id: locvars
      type: locvar
      repeat: expr
      repeat-expr: size_locvars
    - id: size_upvalue_names
      type: u4
    - id: upvalue_names
      type: lua_string
      repeat: expr
      repeat-expr: size_upvalue_names
    types:
      locvar:
        seq:
        - id: varname
          type: lua_string
        - id: start_pc
          type: u4
        - id: end_pc
          type: u4
      upvalue:
        seq:
        - id: in_stack
          type: u1
        - id: idx
          type: u1
      t_value:
        seq:
        - id: t
          type: u1
        - id: v
          type:
            switch-on: t
            cases:
              0: lua_nil
              1: lua_boolean
              3: lua_float
              4: lua_string
              19: lua_integer
              20: lua_string
              _: u1
              
      lua_nil:
        seq:
        - id: nil
          size: 0
      lua_boolean:
        seq:
        - id: v
          type: u1
      lua_float:
        seq:
        - id: v
          type: 
            switch-on: _root.header.sizeof_lua_number
            cases:
              4: f4
              8: f8
      lua_integer:
        seq:
        - id: v
          type: 
            switch-on: _root.header.sizeof_lua_integer
            cases:
              4: u4
              8: u8
      lua_string_size:
        seq:
        - id: b
          type: u1
        - id: x
          if: b == 255
          type: u4
        instances:
          v:
            value: b == 255 ? x : b
      lua_string:
        seq:
        - id: l
          type: lua_string_size
        - id: v
          size: l.v == 0 ? 0 : l.v - 1
      instruction:
        seq:
        - id: v
          size: _root.header.sizeof_instruction
        instances:
          opcode:
            value: v[0] & 0x3f
          ra:
            value: ((v[1] & 0x3f) << 2) + ((v[0] & 0xc0) >> 6)
          rc:
            value: ((v[2] & 0x7f) << 2) + ((v[1] & 0xc0) >> 6)
          rb:
            value: (v[3] << 1) + ((v[2] & 0x80) >> 7)
          ax:
            value: ra + (rc << 8) + (rb << 17)
          bx:
            value: (rb << 9) + rc
          sbx:
            value: bx - ((1<<17) - 1)
      size_t:
        seq:
        - id: v
          type:
            switch-on: _root.header.sizeof_size_t
            cases:
              4: u4
              8: u8
              
  lua_function_52:
    seq:
    - id: line_defined
      type: u4
    - id: last_line_defined
      type: u4
    - id: num_params
      type: u1
    - id: is_vararg
      type: u1
    - id: max_stack_size
      type: u1
    - id: size_code
      type: u4
    - id: code
      type: instruction
      repeat: expr
      repeat-expr: size_code
    - id: size_k
      type: u4
    - id: k
      type: t_value
      repeat: expr
      repeat-expr: size_k
    - id: size_p
      type: u4
    - id: p 
      type: lua_function
      repeat: expr
      repeat-expr: size_p
    - id: size_upvalues
      type: u4
    - id: upvalues
      type: upvalue
      repeat: expr
      repeat-expr: size_upvalues
    - id: source
      type: lua_string
    - id: size_lineinfo
      type: u4
    - id: lineinfo
      type: u4
      repeat: expr
      repeat-expr: size_lineinfo
    - id: size_locvars
      type: u4
    - id: locvars
      type: locvar
      repeat: expr
      repeat-expr: size_locvars
    - id: size_upvalue_names
      type: u4
    - id: upvalue_names
      type: lua_string
      repeat: expr
      repeat-expr: size_upvalue_names
  
    types:
      locvar:
        seq:
        - id: varname
          type: lua_string
        - id: start_pc
          type: u4
        - id: end_pc
          type: u4
      upvalue:
        seq:
        - id: in_stack
          type: u1
        - id: idx
          type: u1
      t_value:
        seq:
        - id: t
          type: u1
        - id: v
          type:
            switch-on: t
            cases:
              0: lua_nil
              1: lua_boolean
              3: lua_number
              4: lua_string
              _: u1
              
      lua_nil:
        seq:
        - id: nil
          size: 0
      lua_boolean:
        seq:
        - id: v
          type: u1
      lua_number:
        seq:
        - id: v
          type: 
            switch-on: _root.header.sizeof_lua_number
            cases:
              4: f4
              8: f8
      lua_string:
        seq:
        - id: l
          type: size_t
        - id: v
          size: l.v
      instruction:
        seq:
        - id: v
          size: _root.header.sizeof_instruction
        instances:
          opcode:
            value: v[0] & 0x3f
          ra:
            value: ((v[1] & 0x3f) << 2) + ((v[0] & 0xc0) >> 6)
          rc:
            value: ((v[2] & 0x7f) << 2) + ((v[1] & 0xc0) >> 6)
          rb:
            value: (v[3] << 1) + ((v[2] & 0x80) >> 7)
          ax:
            value: ra + (rc << 8) + (rb << 17)
          bx:
            value: (rb << 9) + rc
          sbx:
            value: bx - ((1<<17) - 1)
      size_t:
        seq:
        - id: v
          type:
            switch-on: _root.header.sizeof_size_t
            cases:
              4: u4
              8: u8
      