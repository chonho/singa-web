from PIL import Image
import glob, os, sys, random, shutil

size = 32,32


def imToBin(im):
  red = []
  green = []
  blue = []
  pix = im.load()
  for x in range(0,size[0]):
    for y in range(0,size[1]):
      pixel = pix[x,y]
      red.append(pixel[0])
      green.append(pixel[1])
      blue.append(pixel[2])     
  byteArray = bytearray(red+green+blue)
  return byteArray


def fileTo2Bin(file):
  im = Image.open(file)
  byteArray1 = []
  byteArray2 = []
  oldSize = im.size
  #bigest center cube
  data=(0,0,0,0)
  if oldSize[0] < oldSize[1]:
    data= (0,(oldSize[1]-oldSize[0])/2,oldSize[0],(oldSize[1]+oldSize[0])/2)
  else :
    data= ((oldSize[0]-oldSize[1])/2,0,(oldSize[0]+oldSize[1])/2,oldSize[1])
  newIm = im.transform(size,Image.EXTENT,data)
  byteArray1=imToBin(newIm)
  #extent
  data=(0,0,oldSize[0],oldSize[1])
  newIm = im.transform(size,Image.EXTENT,data)
  byteArray2=imToBin(newIm)
  
  return byteArray1,byteArray2
  
def getAllFiles(srcPath):
  files=[]
  for dir in os.listdir(srcPath):  
    if os.path.isdir(os.path.join(srcPath,dir)):
      for file in glob.glob(os.path.join(srcPath,dir,"*.jpg")):
        files.append(file)
  return files


def main():
  srcPath = "images_10"
  targetPath = "images_bin"
  if os.path.exists(targetPath):
    shutil.rmtree(targetPath)
  os.mkdir(targetPath)

  files = getAllFiles(srcPath)
  random.shuffle(files)
  i = 0; j = 0; maxSize = 400
  binFile = open (os.path.join(targetPath,"data_batch_"+str(j)+".bin"),"wb")
  for file in files:
    label = file.split("/")[-1].split("_")[0]
    label = int (label)
    #labelByteArray is 2 bytes 
    labelByteArray = bytearray([(label & 0xff00) >> 8,label & 0x00ff])
    try:
      bin2 = fileTo2Bin(file)
      binFile.write(labelByteArray)
      binFile.write(bin2[0])
      binFile.write(labelByteArray)
      binFile.write(bin2[1])
    except:
      print sys.exc_info()[0]
      continue 
    i += 1 
    if i == maxSize :
      print str(j*10000)+"files generate"
      i = 0
      j += 1
      binFile = open(os.path.join(targetPath,"data_batch_"+str(j)+".bin"),"wb")
  return

def test():
  file = "images/1/1_131.jpg"
  bin2 = fileTo2Bin(file)
  print type(bin2[0])  

  return


main()


 
