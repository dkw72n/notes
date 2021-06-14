meta:
  id: arestype
  file-extension: xml
  endian: le
doc: |
  https://cs.android.com/android/platform/superproject/+/master:frameworks/base/libs/androidfw/include/androidfw/ResourceTypes.h;l=201;drc=master;bpv=1;bpt=1
seq:
  - id: hdr
    type: res_chunk
types:
  file_remainder:
    seq:
    - id: remainder
      size-eos: true
      doc: we don't need this to build plaintext XML
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
      size: size - 8
      type: 
        switch-on: type
        cases:
          res_type::res_xml_type: res_xml_type_remain
          res_type::res_xml_start_namespace_type: res_xml_tree_start_namespace_remain
          res_type::res_xml_start_element_type: res_xml_tree_start_element_remain
          res_type::res_xml_end_element_type: res_xml_tree_end_element_remain
          res_type::res_xml_cdata_type: res_xml_tree_cdata_remain
          res_type::res_string_pool_type: res_string_pool_remain
          res_type::res_xml_resource_map_type: res_xml_resource_map_remain
          _: file_remainder
    
  res_string_pool_remain:
    seq:
    - id: extra
      type: res_string_pool_header
    - id: body
      type: res_string_pool_ext(extra.string_count)
      
  res_xml_type_remain:
    seq:
    - id: chunks
      type: res_chunk
      repeat: eos
      
  res_xml_tree_start_namespace_remain:
    seq:
    - id: extra
      type: res_xml_tree_node
    - id: body
      type: res_xml_tree_start_namespace_ext
  
  res_xml_tree_start_element_remain:
    seq:
    - id: extra
      type: res_xml_tree_node
    - id: body
      type: res_xml_tree_attr_ext
  
  res_xml_tree_end_element_remain:
    seq:
    - id: extra
      type: res_xml_tree_node
    - id: body
      type: res_xml_tree_end_element_ext
  
  res_xml_tree_cdata_remain:
    seq:
    - id: extra
      type: res_xml_tree_node
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
    params:
      - id: string_count
        type: u4
    seq:
    - id: entries
      type: u4
      repeat: expr
      repeat-expr: string_count
    - id: strings
      type: string16
      repeat: expr
      repeat-expr: string_count
    - id: remain
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
      
  res_xml_tree_start_namespace_ext:
    seq:
    - id: prefix
      type: u4
    - id: uri
      type: u4
      
  res_xml_tree_end_element_ext:
    seq:
    - id: ns
      type: u4
    - id: name
      type: u4
      
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
    - id: name
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
      
  res_xml_tree_attribute:
    seq:
    - id: ns
      type: u4
    - id: name
      type: u4
    - id: raw_value
      type: u4
    - id: typed_value
      type: res_value
      
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
      
  res_xml_tree_node:
    seq:
    - id: line_number
      type: u4
    - id: comment
      type: u4
      
  string16:
    seq:
      - id: len
        type: u2
        doc: length of utf16 string
      - id: string
        type: str
        encoding: UTF-16LE
        size: len*2 + 2
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
    
    
