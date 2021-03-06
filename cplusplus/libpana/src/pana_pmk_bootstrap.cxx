/* BEGIN_COPYRIGHT                                                        */
/*                                                                        */
/* Open Diameter: Open-source software for the Diameter and               */
/*                Diameter related protocols                              */
/*                                                                        */
/* Copyright (C) 2002-2007 Open Diameter Project                          */
/*                                                                        */
/* This library is free software; you can redistribute it and/or modify   */
/* it under the terms of the GNU Lesser General Public License as         */
/* published by the Free Software Foundation; either version 2.1 of the   */
/* License, or (at your option) any later version.                        */
/*                                                                        */
/* This library is distributed in the hope that it will be useful,        */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      */
/* Lesser General Public License for more details.                        */
/*                                                                        */
/* You should have received a copy of the GNU Lesser General Public       */
/* License along with this library; if not, write to the Free Software    */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307    */
/* USA.                                                                   */
/*                                                                        */
/* In addition, when you copy and redistribute some or the entire part of */
/* the source code of this software with or without modification, you     */
/* MUST include this copyright notice in each copy.                       */
/*                                                                        */
/* If you make any changes that are appeared to be useful, please send    */
/* sources that include the changed part to                               */
/* diameter-developers@lists.sourceforge.net so that we can reflect your  */
/* changes to one unified version of this software.                       */
/*                                                                        */
/* END_COPYRIGHT                                                          */

#include "pana_pmk_bootstrap.h"
#include "od_utl_sha1.h"

#define PANA_PMK_DEBUG 0

/*
IEEE 802.11i PMK Derivation Algorithm

1.  Introduction

The Extensible Authentication Protocol (EAP) with use of an
authentication method that is capable of generating keys defines a key
management framework.  In the EAP key management framework a key
generated by the authentication method can be exported to derive
another key with which a security association for protecting data
traffic can be dynamically established between the EAP peer and a
third party entity such as a Network Access Server (NAS).  This
document specifies an algorithm for deriving a Pair-wise Master Key
(PMK) of IEEE 802.11i from a AAA-Key or an EAP Master Secret Key
(MSK).  The derived PMK is is used for 4-way handshake operation
between an supplicant and authenticator of IEEE 802.11i to establish a
PTK (Pair-wise Transient Key).

The PMK derivation algorithm is defined mainly for the case where PANA
is used for a network access authentication protocol instead of IEEE
802.1X with IEEE 802.11i where the PMK is the first 32-octet of the
AAA-Key or a EAP Master Shared Key (MSK).  The reason for not simply
using the first 32-octet of the AAA-Key as the PMK is that there may
be multiple IEEE 802.11i access points managed by a single PANA
authentication agent and the supplicant may need a distinct and
cryptographically separated PMK for each access point.


2.  Key Hierarchy


      EAP Peer/             AP/                                  EAP Server
     IEEE 802.1X        IEEE 802.1X            NAS
     Supplicant         Authenticator         (PAA)
+-------------------+                                      +-------------------+
| EAP key hierarchy |                                      | EAP key hierarchy |
|      +---+        |                                      |      +---+        |
|      |MSK|        |                                      |      |MSK|        |
|      +---+        |                                      |      +---+        |
+--------|----------+                                      +--------|----------+
         |                                             AAA-Key      |
         v                                             Transfer     v
      +-------+                              +-------+ Mechanism+-------+
      |AAA-Key|                              |AAA-Key|<---------|AAA-Key|
      +-------+                              +-------+          +-------+
          |                                      |
          v                                      v
+-+-+-+-+-+-+-+-+-+                     +-+-+-+-+-+-+-+-+-+
|  PMK derivation |                     |  PMK derivation |
+-+-+-+-+-+-+-+-+-+                     +-+-+-+-+-+-+-+-+-+
          |                                      |
          v                       PMK Transfer   v
        +---+              +---+   Mechanism   +---+
        |PMK|              |PMK|<--------------|PMK|
        +---+              +---+               +---+
          |                 |
          v                 v
         +-+-+-+-+-+-+-+-+-+-+
         |  4-way handshake  |
         +-+-+-+-+-+-+-+-+-+-+
          |                 |
          v                 v
        +---+             +---+
        |PTK|             |PTK|
        +---+             +---+

The AAA-Key transfer mechanism is used for carrying the derived
AAA-Key from the EAP server to the NAS such as a PAA.  When the EAP
server is implemented in the NAS, the AAA-Key transfer mechanism can
be defined without using an external communication mechanism.
Otherwise, the AAA-Key transfer mechanism is defined by using a AAA
protocol such as RADIUS and Diameter.

The PMK transfer mechanism is used for carrying the derived PMK from
the NAS such as a AAA to the access point.  When the NAS is
implemented in the access point, the PMK transfer mechanism can be
defined without using an external communication mechanism.  Otherwise,
the AAA-Key transfer mechanism is defined by using an configuration
protocol such as SNMP or an proprietary protocol when interoperability
among multiple vendors is not needed.  The PMK transfer mechanism is
out of the scope of this document.


3.  PMK Derivation

The PMK is derived through the key derivation function that is
specified in appendix F of [I-D.ietf-eap-keying].  The KDF function
was defined originally for AMSK derivation, but the PMK derivation
algorithm defined in this document re-uses the same KDF function with
replacing EMSK with AAA-Key.

   PMK = KDF(AAA-Key, key label, optional application data, length)
                                                                                
   KDF (K,L,D,O) = T1 | T2 | T3 | T4 | ...
                                                                                
      where:
      T1 = prf (K, S | 0x01)
      T2 = prf (K, T1 | S | 0x02)
      T3 = prf (K, T2 | S | 0x03)
      T4 = prf (K, T3 | S | 0x04)
                                                                                
      prf = HMAC-SHA1
      K = AAA-Key
      L = key label
      D = application data
      O = OutputLength (2 bytes)
      S = L | " " | D | O
                                                                                
The application specific data for PMK are defined as follows:

- key = AAA-Key

- key label = "IEEE 802.11i PMK derived from AAA-Key"

- application data = SA | AA

  where SA is the MAC address of the IEEE 802.1X supplicant and AA is
  the MAC address of the IEEE 802.1X authenticator.

- output length = 256 bits


References

[I-D.ietf-eap-keying] Aboba, B., Simon, D., Arkko, J., Levkowetz, H.,
"EAP Key Management Framework", draft-ietf-eap-keying-03(work in
progress), July 2004.                                                                              

*/

void PANA_PMKKey::Seed(pana_octetstring_t &aaaKey,
                       pana_octetstring_t &supplicantAddr,   
                       pana_octetstring_t &authenticatorAddr,
                       size_t bit_length)
{
    const int vector_count = (bit_length / 160) + 1;
    char O[2] = { (char)((0xFF00 &bit_length) >> 8), 
                  (char)(0x00FF & bit_length) }; 
    pana_octetstring_t vector;
    pana_octetstring_t &K = aaaKey;
    pana_octetstring_t L = "IEEE 802.11i PMK derived from AAA-Key";
    pana_octetstring_t D = supplicantAddr + authenticatorAddr;
    pana_octetstring_t S = L + " " + D + O;
    pana_octetstring_t *T = new pana_octetstring_t[vector_count];

#if PANA_PMK_DEBUG
    printf("AAA key: %d\n", aaaKey.size());
    for (int i=0; i<(int)aaaKey.size(); i++) {
        printf("%02X ", ((unsigned char*)aaaKey.data())[i]);
    }
    printf("\n");
    printf("Mac supplicant: %d\n", supplicantAddr.size());
    for (int i=0; i<(int)supplicantAddr.size(); i++) {
        printf("%02X ", ((unsigned char*)supplicantAddr.data())[i]);
    }
    printf("\n");
    printf("Mac authenticator: %d\n", authenticatorAddr.size());
    for (int i=0; i<(int)authenticatorAddr.size(); i++) {
        printf("%02X ", ((unsigned char*)authenticatorAddr.data())[i]);
    }
    printf("\n");
#endif

    OD_Utl_Sha1 sha1;
    for (int i = 0; i < vector_count; i ++) {

        char index[2], hash[20];
        ACE_OS::sprintf(index, "%d", i + 1);

        if (i == 0) {
            vector = S + index;
        }
        else {
            vector = T[i-1] + S + index;
        }

        sha1.Update((AAAUInt8*)K.data(), K.size());
        sha1.Update((AAAUInt8*)vector.data(), vector.size());
        sha1.Final();

        ACE_OS::memset(hash, 0x0, sizeof(hash));
        sha1.GetHash((unsigned char*)hash);
        T[i].assign(hash, sizeof(hash));

#if PANA_PMK_DEBUG
        printf("Hash[%d,%d]: ", i, T[i].size());
        for (size_t x=0; x<T[i].size(); x++) {
            printf("%02X ", (unsigned char)T[i].data()[x]);
        }
        printf("\n");
#endif 
    }

    // bit-length value only (20 bytes per vector)
    size_t total_bits = 0;
    char *key = new char[vector_count * 20];
    for (int i = 0; (i < vector_count) && 
         (total_bits < bit_length); i++) {
        memcpy(&key[total_bits/8], T[i].data(), T[i].size());
        total_bits += (T[i].size() * 8);
    }
    m_Key.assign(key, bit_length/8);
    delete key;

#if PANA_PMK_DEBUG
    printf("PMK[%d]: ", m_Key.size());
    for (size_t i=0; i<m_Key.size(); i++) {
        printf("%02X ", (unsigned char)m_Key.data()[i]);
    }
    printf("\n");
#endif 
    delete[] T;
}

