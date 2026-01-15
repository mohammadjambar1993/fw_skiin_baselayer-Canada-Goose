import os
import sys
import subprocess
from zipfile import ZipFile

#general variables
PATH_OUTPUT         = 'C:/Users/Test/Desktop/Heat_project/fw_skiin_baselayer/build/'
PATH_ENCOUTPUT      = 'C:/Users/Test/Desktop/Heat_project/fw_skiin_baselayer/build/encrypted/'
PATH_KEY            = 'C:/Users/Test/Desktop/Bootloader/key/'
ENCRYPTION_KEY      = '8ca10797be50ad9dea019c766d3386c2'
OPENSSL             = 'C:/Progra~1/OpenSSL-Win64/bin/openssl.exe'

#File names for DFU package
BIN_FILE            = 'udw_fw_afib.bin'
ENCRYPTED_BIN_FILE  = 'udw_fw_afib_e.bin'
JSON_FILE           = 'manifest.json'
DAT_FILE            = 'udw_fw_afib.dat'

#paths used to generated application firmware
PATH_APP            = 'C:/Users/Test/Desktop/Heat_project/fw_skiin_baselayer/build/'
FWAPP_OUTPUT        = PATH_OUTPUT+'dfupack.zip'
FWAPP_ENCOUTPUT     = PATH_OUTPUT+'app_baselayer_encrypted.zip'
FWAPP_ENCRYPTIONINP = PATH_ENCOUTPUT+BIN_FILE
FWAPP_ENCRYPTIONOUT = PATH_ENCOUTPUT+ENCRYPTED_BIN_FILE
FWAPP_MANIFEST      = PATH_ENCOUTPUT+JSON_FILE
FWAPP_DATFILE       = PATH_ENCOUTPUT+DAT_FILE
FWAPP_PACKAGE       = PATH_OUTPUT+'imagebuilder_dfupackage_info.txt'

def execute_command(command, args):
    newcmd = args.split(" ")
    newcmd.insert(0, command)
    print("command: {} {}".format(command, args))
    return subprocess.call(newcmd)

def delete_files(filepaths, verbose=False):
    nfiles = len(filepaths)
    for i in range(0, nfiles, 1):
        try:
            if verbose:
                print(filepaths[i])
            os.remove(filepaths[i])
        except FileNotFoundError:
            pass

def encrypt_image():
    #get the nonce input from the init package
    with open(FWAPP_PACKAGE, 'r') as f:
        last_line = f.readlines()[-3]
    #print("last line {}".format(last_line))
    #print("nonce from line {}".format(last_line[-26:-2]))
    temp = bytearray.fromhex(last_line[-26:-2])
    temp.reverse()
    nonce = ''.join(format(x, '02x') for x in temp)
    nonce = nonce+'00000000'
    #print(nonce)
    #Remove all the old files
    print("Removing old output files:")
    delete_files([FWAPP_ENCRYPTIONINP], False)   
    delete_files([FWAPP_ENCRYPTIONOUT], False)   
    delete_files([FWAPP_MANIFEST], False)   
    delete_files([FWAPP_DATFILE], False)   
    
    #extract the output zip file
    print("Extracting {}".format(FWAPP_OUTPUT))
    zip = ZipFile(FWAPP_OUTPUT)
    zip.extractall(PATH_ENCOUTPUT)
    zip.close()
    
    #encrypt the bin file
    print("encrypting..")
    args = 'enc -e -aes-128-ctr -in {} -out {} -K {} -iv {} -v'.format(FWAPP_ENCRYPTIONINP, FWAPP_ENCRYPTIONOUT, ENCRYPTION_KEY, nonce)
    retcode = execute_command(OPENSSL, args)
    if  retcode != 0:            
        print("[ERROR] displaying zip package: ", retcode)
        return False
    
    #create the final encrypted image
    delete_files([FWAPP_ENCRYPTIONINP], False) 
    os.rename(FWAPP_ENCRYPTIONOUT,FWAPP_ENCRYPTIONINP)

    # create a ZipFile object
    delete_files([FWAPP_ENCOUTPUT], False)
    zipObj = ZipFile(FWAPP_ENCOUTPUT, 'w')
    # Add multiple files to the zip
    zipObj.write(FWAPP_ENCRYPTIONINP, BIN_FILE)#renamed
    zipObj.write(FWAPP_MANIFEST, JSON_FILE)
    zipObj.write(FWAPP_DATFILE, DAT_FILE)
    # close the Zip File
    zipObj.close()
    print("Removing temporary files...")
    delete_files([FWAPP_ENCRYPTIONINP], False)   
    delete_files([FWAPP_MANIFEST], False)   
    delete_files([FWAPP_DATFILE], False)  
    os.rmdir(PATH_ENCOUTPUT)
    delete_files([FWAPP_PACKAGE], False)   
    print("Encrypted Package generated")
    return True    

if __name__ == "__main__":
    encrypt_image()