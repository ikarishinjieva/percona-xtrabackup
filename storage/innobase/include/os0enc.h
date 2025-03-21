/***********************************************************************

Copyright (c) 2019, 2021, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

***********************************************************************/

/** @file include/os0enc.h
 Page encryption infrastructure. */

#ifndef os0enc_h
#define os0enc_h

#include <mysql/components/my_service.h>
#include "my_aes.h"
#include "univ.i"

namespace innobase {
namespace encryption {

bool init_keyring_services(SERVICE_TYPE(registry) * reg_srv);

void deinit_keyring_services(SERVICE_TYPE(registry) * reg_srv);
}  // namespace encryption
}  // namespace innobase

// Forward declaration.
class IORequest;
struct Encryption_key;

/** Encryption algorithm. */
class Encryption {
 public:
  /** Algorithm types supported */
  enum Type {

    /** No encryption */
    NONE = 0,

    /** Use AES */
    AES = 1,
  };

  /** Encryption information format version */
  enum Version {

    /** Version in 5.7.11 */
    VERSION_1 = 0,

    /** Version in > 5.7.11 */
    VERSION_2 = 1,

    /** Version in > 8.0.4 */
    VERSION_3 = 2,
  };

  /** Encryption progress type. */
  enum class Progress {
    /* Space encryption in progress */
    ENCRYPTION,
    /* Space decryption in progress */
    DECRYPTION,
    /* Nothing in progress */
    NONE
  };

  /** Encryption operation resume point after server restart. */
  enum class Resume_point {
    /* Resume from the beginning. */
    INIT,
    /* Resume processing. */
    PROCESS,
    /* Operation has ended. */
    END,
    /* All done. */
    DONE
  };

  /** Encryption magic bytes for 5.7.11, it's for checking the encryption
  information version. */
  static constexpr char KEY_MAGIC_V1[] = "lCA";

  /** Encryption magic bytes for 5.7.12+, it's for checking the encryption
  information version. */
  static constexpr char KEY_MAGIC_V2[] = "lCB";

  /** Encryption magic bytes for 8.0.5+, it's for checking the encryption
  information version. */
  static constexpr char KEY_MAGIC_V3[] = "lCC";

  /** Encryption magic bytes for not yet flushed page */
  static constexpr char KEY_MAGIC_EMPTY[] = "\0\0\0";

  /** Encryption master key prifix */
  static constexpr char MASTER_KEY_PREFIX[] = "INNODBKey";

  /** Encryption key length */
  static constexpr size_t KEY_LEN = 32;

  /** Default master key for bootstrap */
  static constexpr char DEFAULT_MASTER_KEY[] = "DefaultMasterKey";

  /** Encryption magic bytes size */
  static constexpr size_t MAGIC_SIZE = 3;

  static const size_t SERVER_UUID_HEX_LEN = 16;
  static constexpr char KEY_MAGIC_PS_V3[] = "PSC";

  /* CRYPT_DATA in ENCRYPTION='N' tablespaces always have unencrypted scheme */
  static const uint CRYPT_SCHEME_UNENCRYPTED = 0;
  /* KEYRING ENCRYPTION header size. aka crypt_data. Also present in
  ENCRYPTION=N Tablespaces */
  static const uint CRYPT_SCHEME_1_IV_LEN = 16;
  static const size_t KEYRING_VALIDATION_TAG_SIZE = MY_AES_BLOCK_SIZE;
  static constexpr uint KEYRING_INFO_MAX_SIZE =
      MAGIC_SIZE + 1                  // type
      + 4                             // min_key_version
      + 4                             // max_key_version
      + 4                             // key_id
      + 1                             // encryption
      + CRYPT_SCHEME_1_IV_LEN         // iv (16 bytes)
      + 1                             // encryption rotation type
      + KEY_LEN                       // tablespace key
      + SERVER_UUID_HEX_LEN           // server's UUID written in hex
      + KEYRING_VALIDATION_TAG_SIZE;  // validation tag

  /** Encryption master key prifix size */
  static constexpr size_t MASTER_KEY_PRIFIX_LEN = 9;

  /** Encryption master key prifix size */
  static constexpr size_t MASTER_KEY_NAME_MAX_LEN = 100;

  /** UUID of server instance, it's needed for composing master key name */
  static constexpr size_t SERVER_UUID_LEN = 36;

  /** BEGIN PS encryption specify */
  static constexpr byte ENCRYPTION_KEYRING_VALIDATION_TAG[] = {
      'E', 'N', 'C', '_', 'V', 'A', 'L', '_',
      'T', 'A', 'G', '_', 'V', '1', '_', '1'};

  static constexpr size_t ENCRYPTION_KEYRING_VALIDATION_TAG_SIZE =
      MY_AES_BLOCK_SIZE;
  static_assert(
      sizeof(ENCRYPTION_KEYRING_VALIDATION_TAG) == MY_AES_BLOCK_SIZE,
      "Size of ENCRYPTION_KEYRING_VALIDATION_TAG must be equal to size "
      "of the output of AES crypto, i.e. MY_AES_BLOCK_SIZE");

  static constexpr uint KERYING_ENCRYPTION_INFO_MAX_SIZE =
      MAGIC_SIZE + 1                             // type
      + 4                                        // min_key_version
      + 4                                        // max_key_version
      + 4                                        // key_id
      + 1                                        // encryption
      + CRYPT_SCHEME_1_IV_LEN                    // iv (16 bytes)
      + 1                                        // encryption rotation type
      + KEY_LEN                                  // tablespace key
      + SERVER_UUID_HEX_LEN                      // server's UUID written in hex
      + ENCRYPTION_KEYRING_VALIDATION_TAG_SIZE;  // validation tag

  static constexpr uint KERYING_ENCRYPTION_INFO_MAX_SIZE_V2 =
      MAGIC_SIZE + 1           // type
      + 4                      // min_key_version
      + 4                      // key_id
      + 1                      // encryption
      + CRYPT_SCHEME_1_IV_LEN  // iv (16 bytes)
      + 1                      // encryption rotation type
      + KEY_LEN                // tablespace key
      + SERVER_UUID_LEN;       // server's UUID

  static constexpr uint KERYING_ENCRYPTION_INFO_MAX_SIZE_V1 =
      MAGIC_SIZE + 2           // length of iv
      + 4                      // space id
      + 2                      // offset
      + 1                      // type
      + 4                      // min_key_version
      + 4                      // key_id
      + 1                      // encryption
      + CRYPT_SCHEME_1_IV_LEN  // iv (16 bytes)
      + 4                      // encryption rotation type
      + KEY_LEN                // tablespace key
      + KEY_LEN;               // tablespace iv
  /** END - PS encryption specify */

  /** Encryption information total size: magic number + master_key_id +
  key + iv + server_uuid + checksum */
  static constexpr size_t INFO_SIZE =
      (MAGIC_SIZE + sizeof(uint32) + (KEY_LEN * 2) + SERVER_UUID_LEN +
       sizeof(uint32));

  /** Maximum size of Encryption information considering all
  formats v1, v2 & v3. */
  static constexpr size_t INFO_MAX_SIZE = INFO_SIZE + sizeof(uint32);

  /** Default master key id for bootstrap */
  static constexpr uint32_t DEFAULT_MASTER_KEY_ID = 0;

  /** (De)Encryption Operation information size */
  static constexpr size_t OPERATION_INFO_SIZE = 1;

  /** Encryption Progress information size */
  static constexpr size_t PROGRESS_INFO_SIZE = sizeof(uint);

  /** Flag bit to indicate if Encryption/Decryption is in progress */
  static constexpr size_t ENCRYPT_IN_PROGRESS = 1 << 0;

  /** Decryption in progress. */
  static constexpr size_t DECRYPT_IN_PROGRESS = 1 << 1;

  /** Tablespaces whose key needs to be reencrypted */
  static std::vector<space_id_t> s_tablespaces_to_reencrypt;

  /** Default constructor */
  Encryption() noexcept : m_type(NONE) {}

  /** Specific constructor
  @param[in]  type    Algorithm type */
  explicit Encryption(Type type) noexcept : m_type(type) {
#ifdef UNIV_DEBUG
    switch (m_type) {
      case NONE:
      case AES:

      default:
        ut_error;
    }
#endif /* UNIV_DEBUG */
  }

  /** Copy constructor */
  Encryption(const Encryption &other) noexcept = default;

  Encryption &operator=(const Encryption &) = default;

  static void set_master_key(ulint master_key_id);

  /** Check if page is encrypted page or not
  @param[in]  page  page which need to check
  @return true if it is an encrypted page */
  [[nodiscard]] static bool is_encrypted_page(const byte *page) noexcept;

  /** Check if a log block is encrypted or not
  @param[in]  block block which need to check
  @return true if it is an encrypted block */
  [[nodiscard]] static bool is_encrypted_log(const byte *block) noexcept;

  /** Check the encryption option and set it
  @param[in]      option      encryption option
  @param[in,out]  type        The encryption type
  @return DB_SUCCESS or DB_UNSUPPORTED */
  [[nodiscard]] dberr_t set_algorithm(const char *option,
                                      Encryption *type) noexcept;

  /** Validate the algorithm string.
  @param[in]  option  Encryption option
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t validate(const char *option) noexcept;

  /** Convert to a "string".
  @param[in]  type  The encryption type
  @return the string representation */
  [[nodiscard]] static const char *to_string(Type type) noexcept;

  /** Check if the string is "empty" or "none".
  @param[in]  algorithm  Encryption algorithm to check
  @return true if no algorithm requested */
  [[nodiscard]] static bool is_none(const char *algorithm) noexcept;

  /** Generate random encryption value for key and iv.
  @param[in,out]  value Encryption value */
  static void random_value(byte *value) noexcept;

  /** Create new master key for key rotation.
  @param[in,out]  master_key  master key */
  static void create_master_key(byte **master_key) noexcept;

  /** Get master key by key id.
  @param[in]      master_key_id master key id
  @param[in]      srv_uuid      uuid of server instance
  @param[in,out]  master_key    master key */
  static void get_master_key(uint32_t master_key_id, char *srv_uuid,
                             byte **master_key) noexcept;

  /** Get current master key and key id.
  @param[in,out]  master_key_id master key id
  @param[in,out]  master_key    master key */
  static void get_master_key(uint32_t *master_key_id,
                             byte **master_key) noexcept;

  /** Fill the encryption information.
  @param[in]      key           encryption key
  @param[in]      iv            encryption iv
  @param[in,out]  encrypt_info  encryption information
  @param[in]      encrypt_key   encrypt with master key
  @return true if success. */
  static bool fill_encryption_info(const byte *key, const byte *iv,
                                   byte *encrypt_info,
                                   bool encrypt_key) noexcept;

  /** Get master key from encryption information
  @param[in]      encrypt_info  encryption information
  @param[in]      version       version of encryption information
  @param[in,out]  m_key_id      master key id
  @param[in,out]  srv_uuid      server uuid
  @param[in,out]  master_key    master key
  @return position after master key id or uuid, or the old position
  if can't get the master key. */
  static byte *get_master_key_from_info(byte *encrypt_info, Version version,
                                        uint32_t *m_key_id, char *srv_uuid,
                                        byte **master_key) noexcept;

  /** Decoding the encryption info from the first page of a tablespace.
  @param[in]      space_id        Tablespace id
  @param[in,out]  e_key           key, iv
  @param[in]      encryption_info encryption info
  @param[in]      decrypt_key     decrypt key using master key
  @return true if success */
  static bool decode_encryption_info(space_id_t space_id, Encryption_key &e_key,
                                     byte *encryption_info,
                                     bool decrypt_key) noexcept;

  /** Encrypt the redo log block.
  @param[in]      type      IORequest
  @param[in,out]  src_ptr   log block which need to encrypt
  @param[in,out]  dst_ptr   destination area
  @return true if success. */
  bool encrypt_log_block(const IORequest &type, byte *src_ptr,
                         byte *dst_ptr) noexcept;

  /** Encrypt the redo log data contents.
  @param[in]      type      IORequest
  @param[in,out]  src       page data which need to encrypt
  @param[in]      src_len   size of the source in bytes
  @param[in,out]  dst       destination area
  @param[in,out]  dst_len   size of the destination in bytes
  @return buffer data, dst_len will have the length of the data */
  byte *encrypt_log(const IORequest &type, byte *src, ulint src_len, byte *dst,
                    ulint *dst_len) noexcept;

  /** Encrypt the page data contents. Page type can't be
  FIL_PAGE_ENCRYPTED, FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
  FIL_PAGE_ENCRYPTED_RTREE.
  @param[in]      type      IORequest
  @param[in,out]  src       page data which need to encrypt
  @param[in]      src_len   size of the source in bytes
  @param[in,out]  dst       destination area
  @param[in,out]  dst_len   size of the destination in bytes
  @return buffer data, dst_len will have the length of the data */
  [[nodiscard]] byte *encrypt(const IORequest &type, byte *src, ulint src_len,
                              byte *dst, ulint *dst_len) noexcept;

  /** Decrypt the log block.
  @param[in]      type  IORequest
  @param[in,out]  src   data read from disk, decrypted data
                        will be copied to this page
  @param[in,out]  dst   scratch area to use for decryption
  @return DB_SUCCESS or error code */
  dberr_t decrypt_log_block(const IORequest &type, byte *src,
                            byte *dst) noexcept;

  /** Decrypt the log data contents.
  @param[in]      type      IORequest
  @param[in,out]  src       data read from disk, decrypted data
                            will be copied to this page
  @param[in]      src_len   source data length
  @param[in,out]  dst       scratch area to use for decryption
  @return DB_SUCCESS or error code */
  dberr_t decrypt_log(const IORequest &type, byte *src, ulint src_len,
                      byte *dst) noexcept;

  /** Decrypt the page data contents. Page type must be
  FIL_PAGE_ENCRYPTED, FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
  FIL_PAGE_ENCRYPTED_RTREE, if not then the source contents are
  left unchanged and DB_SUCCESS is returned.
  @param[in]      type    IORequest
  @param[in,out]  src     data read from disk, decrypt
                          data will be copied to this page
  @param[in]      src_len source data length
  @param[in,out]  dst     scratch area to use for decrypt
  @param[in]  dst_len     size of the scratch area in bytes
  @return DB_SUCCESS or error code */
  [[nodiscard]] dberr_t decrypt(const IORequest &type, byte *src, ulint src_len,
                                byte *dst, ulint dst_len) noexcept;

  /** Check if keyring plugin loaded. */
  static bool check_keyring() noexcept;

  /** Get encryption type
  @return encryption type **/
  Type get_type() const;

  /** Check if the encryption algorithm is NONE.
  @return true if no algorithm is set, false otherwise. */
  [[nodiscard]] bool is_none() const noexcept { return m_type == NONE; }

  /** Set encryption type
  @param[in]  type  encryption type **/
  void set_type(Type type);

  /** Set encryption key
  @param[in]  key  encryption key **/
  void set_key(const byte *key);

  /** Get key length
  @return  key length **/
  ulint get_key_length() const;

  /** Set key length
  @param[in]  klen  key length **/
  void set_key_length(ulint klen);

  /** Set initial vector
  @param[in]  iv  initial_vector **/
  void set_initial_vector(const byte *iv);

  /** Get master key id
  @return master key id **/
  static uint32_t get_master_key_id();

 private:
  /** Encrypt the page data contents. Page type can't be
  FIL_PAGE_ENCRYPTED, FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
  FIL_PAGE_ENCRYPTED_RTREE.
  @param[in]  src       page data which need to encrypt
  @param[in]  src_len   size of the source in bytes
  @param[in,out]  dst       destination area
  @param[in,out]  dst_len   size of the destination in bytes
  @return true if operation successful, false otherwise. */
  [[nodiscard]] bool encrypt_low(byte *src, ulint src_len, byte *dst,
                                 ulint *dst_len) noexcept;

  /** Encrypt type */
  Type m_type;

  /** Encrypt key */
  const byte *m_key;

  /** Encrypt key length*/
  ulint m_klen;

  /** Encrypt initial vector */
  const byte *m_iv;

  /** Current master key id */
  static uint32_t s_master_key_id;

  /** Current uuid of server instance */
  static char s_uuid[SERVER_UUID_LEN + 1];
};

struct Encryption_key {
  /** Encrypt key */
  byte *m_key;

  /** Encrypt initial vector */
  byte *m_iv;

  /** Master key id */
  uint32_t m_master_key_id{Encryption::DEFAULT_MASTER_KEY_ID};
};
#endif /* os0enc_h */
