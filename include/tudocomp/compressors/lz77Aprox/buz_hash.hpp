#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
namespace tdc{

    class buz_hash : public hash_interface{

    private:
        const uint64_t PRIME_BASE;
        const uint64_t PRIME_MOD;


        //http://www.serve.net/buz/hash.adt/java.000.html for info
        //and http://www.java2s.com/Code/Java/Development-Class/AveryefficientjavahashalgorithmbasedontheBuzHashalgoritm.htm as source of table
        const uint64_t tab[256]= {0x4476081a7043a46f, 0x45768b8a6e7eac19, 0xebd556c1cf055952,
                                  0x72ed2da1bf010101, 0x3ff2030b128e8a64,
                                  0xcbc330238adcfef2, 0x737807fe42e20c6c, 0x74dabaedb1095c58,
                                  0x968f065c65361d67, 0xd3f4018ac7a4b199,
                                  0x954b389b52f24df2, 0x2f97a9d8d0549327, 0xb9bea2b49a3b180f,
                                  0xaf2f42536b21f2eb, 0x85d991663cff1325,
                                  0xb9e1260207b575b9, 0xf3ea88398a23b7e2, 0xfaf8c83ffbd9091d,
                                  0x4274fe90834dbdf9, 0x3f20b157b68d6313,
                                  0x68b48972b6d06b93, 0x694837b6eba548af, 0xeecb51d1acc917c9,
                                  0xf1c633f02dffbcfa, 0xa6549ec9d301f3b5,
                                  0x451dc944f1663592, 0x446d6acef6ce9e4f, 0x1c8a5b3013206f02,
                                  0x5908ca36f2dc50f7, 0x4fd55d3f3e880a87,
                                  0xa03a8dbeabbf065d, 0x3ccbbe078fabcb6d, 0x1da53a259116f2d0,
                                  0xfb27a96fcb9af152, 0x50aba242e85aec09,
                                  0x24d4e414fc4fc987, 0x83971844a9ce535e, 0xc26a3fdeb849398e,
                                  0xc2380d044d2e70d8, 0xab418aa8ae19b18f,
                                  0xd95b6b9247d5ebea, 0x8b3b2171fdc60511, 0xe15cd0ae3fcc44af,
                                  0x5a4e27f914a68f17, 0x377bd28ca09aafdc,
                                  0xbbeb9828594a3294, 0x7c8df263ae1de1b9, 0xba0a48a5fd1c1dd0,
                                  0x57cc1b8818b98ee6, 0x8c570975d357dabc,
                                  0x76bdcd6f2e8826aa, 0x529b15b6ec4055f1, 0x9147c7a54c34f8a9,
                                  0x2f96a7728170e402, 0xe46602f455eca72e,
                                  0x22834c4dd1bde03f, 0x2644cf5a25e368ff, 0x907c6de90b120f4a,
                                  0xadfe8ba99028f728, 0xa85199ae14df0433,
                                  0x2d749b946dd3601e, 0x76e35457aa052772, 0x90410bf6e427f736,
                                  0x536ad04d13e35041, 0x8cc0d76769b76914,
                                  0xae0249f6e3b3c01c, 0x1bdfd075307d6faf, 0xd8e04f70c221decc,
                                  0x4ab23622a4281a5d, 0x37a5613da2fcaba7,
                                  0x19a56203666d4a9f, 0x158ffab502c4be93, 0x0bee714e332ecb2f,
                                  0x69b71a59f6f74ab0, 0x0fc7fc622f1dfe8f,
                                  0x513966de7152a6f9, 0xc16fae9cc2ea9be7, 0xb66f0ac586c1899e,
                                  0x11e124aee3bdefd7, 0x86cf5a577512901b,
                                  0x33f33ba6994a1fbd, 0xde6c4d1d3d47ff0d, 0x6a99220dc6f78e66,
                                  0x2dc06ca93e2d25d2, 0x96413b520134d573,
                                  0xb4715ce8e1023afa, 0xe6a75900c8c66c0a, 0x6448f13ad54c12ed,
                                  0xb9057c28cf6689f0, 0xf4023daf67f7677a,
                                  0x877c2650767b9867, 0xb7ea587dcd5b2341, 0xc048cf111733f9bc,
                                  0x112012c15bc867bf, 0xc95f52b1d9418811,
                                  0xa47e624ee7499083, 0x26928606df9b12e8, 0x5d020462ec3e0928,
                                  0x8bbde651f6d08914, 0xd5db83db758e524a,
                                  0x3105e355c000f455, 0xdd7fe1b81a786c79, 0x1f3a818c8e012db1,
                                  0xd902de819d7b42fa, 0x4200e63325cda5f0,
                                  0x0e919cdc5fba9220, 0x5360dd54605a11e1, 0xa3182d0e6cb23e6c,
                                  0x13ee462c1b483b87, 0x1b1b6087b997ee22,
                                  0x81c36d0b877f7362, 0xc24879932c1768d4, 0x1faa756e1673f9ad,
                                  0x61651b24d11fe93d, 0x30fe3d9304e1cde4,
                                  0x7be867c750747250, 0x973e52c7005b5db6, 0x75d6b699bbaf4817,
                                  0x25d2a9e97379e196, 0xe65fb599aca98701,
                                  0x6ac27960d24bde84, 0xdfacc04c9fabbcb6, 0xa46cd07f4a97882b,
                                  0x652031d8e59a1fd8, 0x1185bd967ec7ce10,
                                  0xfc9bd84c6780f244, 0x0a0c59872f61b3ff, 0x63885727a1c71c95,
                                  0x5e88b4390b2d765c, 0xf0005ccaf988514d,
                                  0x474e44280a98e840, 0x32de151c1411bc42, 0x2c4b86d5aa4482c2,
                                  0xccd93deb2d9d47da, 0x3743236ff128a622,
                                  0x42ed2f2635ba5647, 0x99c74afd18962dbd, 0x2d663bb870f6d242,
                                  0x7912033bc7635d81, 0xb442862f43753680,
                                  0x94b1a5400aeaab4c, 0x5ce285fe810f2220, 0xe8a7dbe565d9c0b1,
                                  0x219131af78356c94, 0x7b3a80d130f27e2f,
                                  0xbaa5d2859d16b440, 0x821cfb6935771070, 0xf68cfb6ee9bc2336,
                                  0x18244132e935d2fd, 0x2ed0bda1f4720cff,
                                  0x4ed48cdf6975173c, 0xfd37a7a2520e2405, 0x82c102b2a9e73ce2,
                                  0xadac6517062623a7, 0x5a1294d318e26104,
                                  0xea84fe65c0e4f061, 0x4f96f8a9464cfee9, 0x9831dff8ccdc534a,
                                  0x4ca927cd0f192a14, 0x030900b294b71649,
                                  0x644b263b9aeb0675, 0xa601d4e34647e040, 0x34d897eb397f1004,
                                  0xa6101c37f4ec8dfc, 0xc29d2a8bbfd0006b,
                                  0xc6b07df8c5b4ed0f, 0xce1b7d92ba6bccbe, 0xfa2f99442e03fe1b,
                                  0xd8863e4c16f0b363, 0x033b2cccc3392942,
                                  0x757dc33522d6cf9c, 0xf07b1ff6ce55fec5, 0x1569e75f09b40463,
                                  0xfa33fa08f14a310b, 0x6eb79aa27bbcf76b,
                                  0x157061207c249602, 0x25e5a71fc4e99555, 0x5df1fe93de625355,
                                  0x235b56090c1aa55d, 0xe51068613eaced91,
                                  0x45bd47b893b9ff1e, 0x6595e1798d381f2d, 0xc9b5848cbcdb5ba8,
                                  0x65985146ff7792bc, 0x4ab4a17bf05a19a0,
                                  0xfd94f4ca560ffb0c, 0xcf9bad581a68fa68, 0x92b4f0b502b1ce1a,
                                  0xbcbec0769a610474, 0x8dbd31ded1a0fecb,
                                  0xdd1f5ed9f90e8533, 0x61c1e6a523f84d95, 0xf24475f383c110c4,
                                  0xdb2dffa66f90588d, 0xac06d88e9ee04455,
                                  0xa215fc47c40504ba, 0x86d7caebfee93369, 0x9eaec31985804099,
                                  0x0fba2214abe5d01b, 0x5a32975a4b3865d6,
                                  0x8cceebc98a5c108f, 0x7e12c4589654f2dc, 0xa49ad49fb0d19772,
                                  0x3d142dd9c406152b, 0x9f13589e7be2b8a5,
                                  0x5e8dbac1892967ad, 0xcc23b93a6308e597, 0x1ef35f5fe874e16a,
                                  0x63ae9cc08d2e274f, 0x5bbabee56007fc05,
                                  0xabfd72994230fc39, 0x9d71a13a99144de1, 0xd9daf5aa8dcc89b3,
                                  0xe145ec0514161bfd, 0x143befc2498cd270,
                                  0xa8e192557dbbd9f8, 0xcbeda2445628d7d0, 0x997f0a93205d9ea4,
                                  0x01014a97f214ebfa, 0x70c026ffd1ebedaf,
                                  0xf8737b1b3237002f, 0x8afcbef3147e6e5e, 0x0e1bb0684483ebd3,
                                  0x4cbad70ae9b05aa6, 0xd4a31f523517c363,
                                  0xdb0f057ae8e9e8a2, 0x400894a919d89df6, 0x6a626a9b62defab3,
                                  0xf907fd7e14f4e201, 0xe10e4a5657c48f3f,
                                  0xb17f9f54b8e6e5dc, 0x6b9e69045fa6d27a, 0x8b74b6a41dc3078e,
                                  0x027954d45ca367f9, 0xd07207b8fdcbb7cc,
                                  0xf397c47d2f36414b, 0x05e4e8b11d3a034f, 0x36adb3f7122d654f,
                                  0x607d9540eb336078, 0xb639118e3a8b9600,
                                  0xd0a406770b5f1484, 0x3cbee8213ccfb7c6, 0x467967bb2ff89cf1,
                                  0xb115fe29609919a6, 0xba740e6ffa83287e,
                                  0xb4e51be9b694b7cd, 0xc9a081c677df5aea, 0x2e1fbcd8944508cc,
                                  0xf626e7895581fbb8, 0x3ce6e9b5728a05cb,
                                  0x46e87f2664a31712, 0x8c1dc526c2f6acfa, 0x7b4826726e560b10,
                                  0x2966e0099d8d7ce1, 0xbb0dd5240d2b2ade, 0x0d527cc60bbaa936};

    public:

        buz_hash():PRIME_BASE(0),PRIME_MOD(0){

        }



        ~buz_hash(){};


        inline uint64_t rotate(uint64_t val, uint64_t exp) {
            return (val << exp | val >> (64 - exp));
        }

        inline uint64_t make_hash(len_t start, len_t size,io::InputView &input_view){


            uint64_t hash =PRIME_MOD;
            uint64_t byte ;
            for (size_t i = 0; i < size; i++){
                byte = tab[input_view[start+i]];
                hash = rotate(hash,1);
                hash = hash ^ byte;
            }

            return hash;
        }

        inline rolling_hash make_rolling_hash(len_t start, len_t size,io::InputView &input_view){
            rolling_hash rhash;
            rhash.length=size;
            rhash.position=start;
            rhash.hashvalue=make_hash(start,size,input_view);

            rhash.c0_exp=1;

            return rhash;
        }

        inline void advance_rolling_hash(rolling_hash &rhash,io::InputView &input_view){

            rhash.hashvalue = rotate(rhash.hashvalue,1) ^ rotate(tab[input_view[rhash.position]],rhash.length) ^  tab[input_view[rhash.position+rhash.length]];

            rhash.position++;
            /*
             if(rhash.hashvalue!=make_hash(rhash.position,rhash.length,input_view)){
                std::cout<<input_view.substr(rhash.position,rhash.length)<<std::endl;
              std::cout<<"rhash: "<<rhash.hashvalue<<" ,mhash: "<<make_hash(rhash.position,rhash.length,input_view)<<std::endl;

                std::cout<<"old hash: "<<make_hash(rhash.position-1,rhash.length,input_view)<<std::endl;
                std::cout<<"pos: "<<rhash.position<<" l: "<<rhash.length<<std::endl;
               std::cout<<"int: "<<int(input_view[rhash.position+rhash.length-1])<<" int(char): "<<int(char(input_view[rhash.position+rhash.length-1]))<<std::endl;
                std::cout<<"ascii: "<<input_view[rhash.position-1]<<" int: "<<int(input_view[rhash.position-1])<<std::endl;

              throw std::invalid_argument( "doesnt match hash" );
            }
            */


        }



    };}