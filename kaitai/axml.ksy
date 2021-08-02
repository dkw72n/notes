meta:
  id: arestype
  file-extension: xml
  endian: le
doc: |
  https://cs.android.com/android/platform/superproject/+/master:frameworks/base/libs/androidfw/include/androidfw/ResourceTypes.h;l=201;drc=master;bpv=1;bpt=1
seq:
  - id: magic
    contents: [0x03, 0x00]
    doc: 3 denotes to RES_XML_TYPE
  - id: header_size
    type: u2
    doc: header length
  - id: size
    type: u4
    doc: length of manifest file
  - id: chunks
    type: res_chunk
    repeat: eos
    
types:
  file_remainder:
    seq:
    - id: remainder
      size-eos: true
      doc: we don't need this to build plaintext XML
      
  res_chunk_header:
    seq:
    - id: type
      type: u2
      enum: res_type
      doc: 3 denotes to RES_XML_TYPE
    - id: header_size
      type: u2
      doc: header length
    - id: size
      type: u4
      doc: length of manifest file
      
  res_chunk:
    doc: https://cs.android.com/android/platform/superproject/+/master:frameworks/base/libs/androidfw/include/androidfw/ResourceTypes.h;drc=master;l=201
    seq:
    - id: type
      type: u2
      enum: res_type
      doc: 3 denotes to RES_XML_TYPE
    - id: header_size
      type: u2
      doc: header length
    - id: size
      type: u4
      doc: length of manifest file
    
    - id: x
      size: size - 8 # header_size
      type: 
        switch-on: type
        cases:
          res_type::res_xml_type: res_xml_type_remain
          res_type::res_xml_start_namespace_type: res_xml_tree_namespace_remain
          res_type::res_xml_start_element_type: res_xml_tree_start_element_remain
          res_type::res_xml_end_element_type: res_xml_tree_end_element_remain
          res_type::res_xml_end_namespace_type: res_xml_tree_namespace_remain
          res_type::res_xml_cdata_type: res_xml_tree_cdata_remain
          res_type::res_string_pool_type: res_string_pool_remain
          res_type::res_xml_resource_map_type: res_xml_resource_map_remain
          _: file_remainder
    
    instances:
      tag:
        value: type == res_type::res_xml_start_element_type ? x.as<res_xml_tree_start_element_remain>.tag : type == res_type::res_xml_end_element_type ? x.as<res_xml_tree_end_element_remain>.tag : ""
      tag_prefix:
        value: type == res_type::res_xml_start_element_type ? "<" : type == res_type::res_xml_end_element_type ? "</" : ""
      tag_postfix:
        value: type == res_type::res_xml_start_element_type ? ">" : type == res_type::res_xml_end_element_type ? ">" : ""
    
    -webide-representation: "{tag_prefix}{tag}{tag_postfix}"
  res_string_pool_remain:
    seq:
    - id: extra
      type: res_string_pool_header
    - id: body
      type: res_string_pool_ext
      
  res_xml_type_remain:
    seq:
    - id: chunks
      type: res_chunk
      repeat: eos
      
  res_xml_tree_namespace_remain:
    seq:
    - id: line_number
      type: u4
    - id: comment
      type: u4
    - id: body
      type: res_xml_tree_start_namespace_ext
  
    instances:
      prefix:
        value: body.prefix.n
      uri:
        value: body.uri.n
    -webide-representation: "xlmns:{prefix}={uri}"
    
  res_xml_tree_start_element_remain:
    seq:
    - id: line_number
      type: u4
    - id: comment
      type: u4
    - id: ns
      type: u4
    - id: name_idx
      type: u4
    - id: attribute_start
      type: u2
    - id: attribute_size
      type: u2      
    - id: attribute_count
      type: u2
    - id: id_index
      type: u2
    - id: class_index
      type: u2
    - id: style_index
      type: u2
    - id: extra
      type: file_remainder
      size: 20 - attribute_start
    - id: attributes
      type: res_xml_tree_attribute
      repeat: expr
      repeat-expr: attribute_count
    - id: should_be_empty
      type: file_remainder
    instances:
      tag:
        value: _root.chunks[0].x.as<res_string_pool_remain>.body.entries[name_idx].n
    -webide-representation: "<{tag}>"
  
  res_xml_tree_end_element_remain:
    seq:
    - id: line_number
      type: u4
    - id: comment
      type: u4
    - id: body
      type: res_xml_tree_end_element_ext
    instances:
      tag:
        value: body.name
    -webide-representation: "</{tag}>"
    
      
  res_xml_tree_cdata_remain:
    seq:
    - id: line_number
      type: u4
    - id: comment
      type: u4
    - id: body
      type: res_xml_tree_cdata_ext
      
  res_xml_resource_map_remain:
    seq:
    - id: res_id
      type: u4
      repeat: eos
      
  res_chunks:
    seq:
    - id: chunks
      type: res_chunk
      repeat: eos
      
  res_string_pool_ext:
    seq:
    - id: entries
      type: string_offset
      repeat: expr
      repeat-expr: _parent.extra.string_count
    - id: strings
      type: file_remainder
      
  res_string_pool_header:
    seq:
    - id: string_count
      type: u4
    - id: style_count
      type: u4
    - id: flags
      type: u4
    - id: strings_start
      type: u4
    - id: styles_start
      type: u4
    
    instances:
      string_bits:
        value: (flags & (1<<8)) == 0?16:8
      is_sorted:
        value: (flags & 1) != 0
        
  res_xml_tree_start_namespace_ext:
    seq:
    - id: prefix
      type: string_index
    - id: uri
      type: string_index
      
  res_xml_tree_end_element_ext:
    seq:
    - id: ns
      type: u4
    - id: name_idx
      type: u4
    instances:
      name:
        value: _root.chunks[0].x.as<res_string_pool_remain>.body.entries[name_idx].n
  
  res_xml_tree_cdata_ext:
    seq:
    - id: data
      type: u4
    - id: typed_data
      type: res_value
      
  res_xml_tree_attr_ext:
    seq:
    - id: ns
      type: u4
    - id: name_idx
      type: u4
    - id: attribute_start
      type: u2
    - id: attribute_size
      type: u2      
    - id: attribute_count
      type: u2
    - id: id_index
      type: u2
    - id: class_index
      type: u2
    - id: style_index
      type: u2
    - id: extra
      type: file_remainder
      size: 20 - attribute_start
    - id: attributes
      type: res_xml_tree_attribute
      repeat: expr
      repeat-expr: attribute_count
    - id: should_be_empty
      type: file_remainder
    instances:
      name:
        value: _root.chunks[0].x.as<res_string_pool_remain>.body.entries[name_idx].n
    
        
  res_xml_tree_attribute:
    seq:
    - id: ns
      type: u4
    - id: name_idx
      type: u4
    - id: raw_value
      type: u4
    - id: typed_value
      type: res_value
    instances:
      key:
        value: _root.chunks[0].x.as<res_string_pool_remain>.body.entries[name_idx].n
    -webide-representation: "{key} = "
    
  res_value:
    seq:
    - id: size
      type: u2
    - id: res0
      type: u1
    - id: data_type
      type: u1
    - id: data
      type: u4
  
  string_x:
    params:
      - id: bits
        type: u1
    seq:
      - id: v
        type:
          switch-on: bits
          cases:
            8: string8
            16: string16
    instances:
      value:
        value: bits == 8 ? v.as<string8>.string : v.as<string16>.string
        
    -webide-representation: "\"{value}\""
  
  string_len8:
    doc:
      https://cs.android.com/android/platform/superproject/+/master:frameworks/base/libs/androidfw/ResourceTypes.cpp;drc=master;l=727
    seq:
      - id: b0
        type: u1
      - id: b1
        type: u1
        if: (b0 & 0x80) != 0
    instances:
      value:
        value: (b0 & 0x80) == 0 ? b0 : ((b0 & 0x7f) * 256 + b1 )
  
  string_len16:
    doc:
      https://cs.android.com/android/platform/superproject/+/master:frameworks/base/libs/androidfw/ResourceTypes.cpp;drc=master;l=727
    seq:
      - id: b0
        type: u2
      - id: b1
        type: u2
        if: (b0 & 0x8000) != 0
    instances:
      value:
        value: (b0 & 0x8000) == 0 ? b0 : ((b0 & 0x7fff) * 65536 + b1 )
        
  string16:
    doc:
      https://cs.android.com/android/platform/superproject/+/android-11.0.0_r1:frameworks/base/libs/androidfw/ResourceTypes.cpp;drc=8a891d86abdf35b6703785fb3368b39510e91357;l=684
    seq:
      - id: len
        type: string_len16
      - id: string
        type: str
        encoding: UTF-16LE
        size: len.value * 2
      - id: trailing
        type: u2
    -webide-representation: "\"{string}\"" 
  
  string8:
    doc:
      https://cs.android.com/android/platform/superproject/+/android-11.0.0_r1:frameworks/base/libs/androidfw/ResourceTypes.cpp;drc=8a891d86abdf35b6703785fb3368b39510e91357;l=744
    seq:
      - id: u16len
        type: string_len8
      - id: u8len
        type: string_len8
      - id: string
        type: str
        encoding: UTF-8
        size: u8len.value * 1
      - id: trailing
        type: u1
    -webide-representation: "\"{string}\""
        
  string_offset:
    seq:
      - id: offset
        type: u4
  
    instances:
      strx:
        io: _root._io 
        pos: offset + _root.chunks[0].x.as<res_string_pool_remain>.extra.strings_start + 8
        type: string_x(_root.chunks[0].x.as<res_string_pool_remain>.extra.string_bits)
      n:
        value: strx.value
        
    -webide-representation: "\"{n}\""
    
  string_index:
    seq:
      - id: idx
        type: u4
  
    instances:
      n:
        value: _root.chunks[0].x.as<res_string_pool_remain>.body.entries[idx].n
        
    -webide-representation: "\"{n}\""
    
  string_pool_span:
    seq:
      - id: name_idx
        type: string_index
      - id: start
        type: u4
      - id: end
        type: u4

enums:
  res_type:
    0: res_null_type
    1: res_string_pool_type
    2: res_table_type
    3: res_xml_type
    256: res_xml_start_namespace_type
    256: res_xml_first_chunk_type
    257: res_xml_end_namespace_type
    258: res_xml_start_element_type
    259: res_xml_end_element_type
    260: res_xml_cdata_type
    383: res_xml_last_chunk_type
    384: res_xml_resource_map_type