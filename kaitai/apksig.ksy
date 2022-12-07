meta:
  id: apksig
  file-extension: apk
  endian: le
  imports:
    - asn1_der
seq:
  - id: x
    size: _io.size - 22
  - id: magic
    contents: 'PK'
  - id: section_type
    contents: [0x05, 0x06]
  - id: body
    type: end_of_central_dir
types:
  end_of_central_dir:
    seq:
      - id: disk_of_end_of_central_dir
        type: u2
      - id: disk_of_central_dir
        type: u2
      - id: num_central_dir_entries_on_disk
        type: u2
      - id: num_central_dir_entries_total
        type: u2
      - id: len_central_dir
        type: u4
      - id: ofs_central_dir
        type: u4
      - id: len_comment
        type: u2
      - id: comment
        type: str
        size: len_comment
        encoding: UTF-8
    instances:
      apk_sig_block_42_tail:
        pos: ofs_central_dir - 24
        type: end_of_apk_sig_block
        
  end_of_apk_sig_block:
    seq:
      - id: len
        type: u8
      - id: magic
        contents: 'APK Sig Block 42'
    instances:
      sig_block:
        pos: _root.body.ofs_central_dir - len - 8
        size: len - 16
        type: sig_block
  sig_block:
    seq:
      - id: length
        type: u8
      - id: id_pairs
        #size: length - 8
        type: id_pair
        repeat: eos
        #repeat-expr: 3
  lpa_body:
    seq:
      - id: children
        type: lpd_lv1
        repeat: eos
  lpa_lv1:
    seq:
      - id: length
        type: u4
      - id: body
        size: length
        type: lpa_body
  lpd_lv1:
    seq:
      - id: length
        type: u4
      - id: body
        size: length
  digest:
    seq:
      - id: length
        type: u4
      - id: algo
        type: u4
      - id: body
        size: length - 4
  digests:
    seq:
      - id: length
        type: u4
      - id: body
        size: length
  certificates:
    seq:
      - id: length
        type: u4
      - id: x
        size: length
        type: body
    types:
      body:
        seq:
          - id: cert
            type: lp_asn1_der
            repeat: eos
  additional_attributes:
    seq:
      - id: length
        type: u4
      - id: body
        size: length
  signer_v2:
    seq:
      - id: length
        type: u4
      - id: signed_data
        type: signed_data_v2
      - id: signatures
        type: signatures
      - id: public_key
        type: lp_asn1_der
  signer_v3:
    seq:
      - id: length
        type: u4
      - id: signed_data
        type: signed_data_v3
      - id: min_sdk
        type: u4
      - id: max_sdk
        type: u4
      - id: signatures
        type: signatures
      - id: public_key
        type: lp_asn1_der
  signed_data_v2:
    seq:
      - id: length
        type: u4
      - id: x
        size: length
        type: body
    types:
      body:
        seq:
          - id: digests
            type: lpa_lv1
          - id: certificates
            type: certificates      
          - id: addition_attrs
            type: lpa_lv1
  signed_data_v3:
    seq:
      - id: length
        type: u4
      - id: x
        size: length
        type: body
    types:
      body:
        seq:
          - id: digests
            type: lpa_lv1
          - id: certificates
            type: certificates
          - id: min_sdk
            type: u4
          - id: max_sdk
            type: u4
          - id: addition_attrs
            type: lpa_lv1
  lp_asn1_der:
    seq:
      - id: length
        type: u4
      - id: body
        size: length
        type: asn1_der
  signature:
    seq:
      - id: length
        type: u4
      - id: algo
        type: u4
      - id: signed_data
        type: lpd_lv1
  signatures:
    seq:
      - id: length
        type: u4
      - id: body
        size: length
        type: signature
        #repeat: eos
  signature_v2:
    doc: https://source.android.com/docs/security/features/apksigning/v2#apk-signature-scheme-v2-block-format
    seq:
      - id: len
        type: u4
      - id: signers
        size: len
        type: signer_v2
        repeat: eos
  signature_v3:
    doc: https://source.android.com/docs/security/features/apksigning/v3#format
    seq:
      - id: len
        type: u4
      - id: x
        size: len
        type: body
    types:
      body:
        seq:
          - id: signers
            type: signer_v3
            repeat: eos
  id_pair:
    seq:
      - id: len
        type: u8
      - id: type
        type: u4
      - id: body
        size: len - 4
        type:
          switch-on: type
          cases:
            0x7109871a: signature_v2
            0xf05368c0: signature_v3
    -webide-representation: "{type} {name}"