

use IO;

proc loadImages(num: int, fileName: string = "week2/emnist/data/train-images-idx3-ubyte") {

    var deserializer = new binaryDeserializer(endianness.big);

    var fr = openReader(fileName, deserializer=deserializer, locking=false);

    var magicNumber = fr.read(int(32));
    if magicNumber != 2051 then {
        writeln("Error: Invalid magic number reading images");
        halt(1);
    }

    var imageCount = fr.read(int(32));
    var rowCount = fr.read(int(32));
    var columnCount = fr.read(int(32));

    var imageDomain = {0..#rowCount, 0..#columnCount};

    proc readImage() {
        var raw: [imageDomain] uint(8);
        for i in 0..<rowCount {
            for j in 0..<columnCount {
                raw[i,j] = fr.read(uint(8));
            }
        }
        // note: this is a 32-bit divide today
        var image: [imageDomain] real = raw / 255.0;
        return image;
    }
    const imageBatchDomain = {0..#num};
    var images: [imageBatchDomain] [imageDomain] real(64);
    for i in images.domain {
        images[i] = readImage();
    }

    return images;

}


proc loadLabels(num: int, fileName: string = "week2/emnist/data/train-labels-idx1-ubyte") {

    var deserializer = new binaryDeserializer(endianness.big);

    var fr = openReader(fileName, deserializer=deserializer, locking=false);

    var magicNumber = fr.read(int(32));
    if magicNumber != 2049 then {
        writeln("Error: Invalid magic number reading labels");
        halt(1);
    }

    var labelCount = fr.read(int(32));

    const imageBatchDomain = {0..#num};

    var labels: [imageBatchDomain] int;
    for i in labels.domain {
        labels[i] = fr.read(int(8));
    }

    var formattedLabels: [imageBatchDomain] [0..#10] real(64);
    for i in labels.domain {
        for j in 0..<10 {
            formattedLabels[i][j] = if labels[i] == j then 1.0 else 0.0;
        }
    }

    return (labels, formattedLabels);
}

proc printImage(image: [0..#28, 0..#28] real) {
    for pixel in image do
        write(if pixel>0.5 then "#" else " ");
    writeln();
}
