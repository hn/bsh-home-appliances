let path = require('path');
let fs = require('fs');
const {minify} = require('html-minifier-terser');
let gzipAsync = require('@gfx/zopfli').gzipAsync;

const SAVE_PATH = '../../include';

function chunkArray(myArray, chunk_size) {
    let index = 0;
    let arrayLength = myArray.length;
    let tempArray = [];
    for (index = 0; index < arrayLength; index += chunk_size) {
        let myChunk = myArray.slice(index, index + chunk_size);
        tempArray.push(myChunk);
    }
    return tempArray;
}

function addLineBreaks(buffer) {
    let data = '';
    let chunks = chunkArray(buffer, 30);
    chunks.forEach((chunk, index) => {
        data += chunk.join(',');
        if (index + 1 !== chunks.length) {
            data += ',\n';
        }
    });
    return data;
}

(async function(){
    const indexHtml = fs.readFileSync(path.resolve(__dirname, './index.html')).toString();
    const indexHtmlMinify = await minify(indexHtml, {
        collapseWhitespace: true,
        removeComments: true,
        removeAttributeQuotes: true,
        removeRedundantAttributes: true,
        removeScriptTypeAttributes: true,
        removeStyleLinkTypeAttributes: true,
        useShortDoctype: true,
        minifyCSS: true,
        minifyJS: true,
        sortAttributes: true, // It won't change the length of the generated HTML, but it will optimize the file size after compression.
        sortClassName: true, // It won't change the length of the generated HTML, but it will optimize the file size after compression.
    });
    console.log(`[finalize.js] Minified index.html | Original Size: ${(indexHtml.length / 1024).toFixed(2) }KB | Minified Size: ${(indexHtmlMinify.length / 1024).toFixed(2) }KB`);

    try{
        const GZIPPED_INDEX = await gzipAsync(indexHtmlMinify, { numiterations: 15 });

        const FILE = 
`
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#define WSL_CUSTOM_PAGE
const uint32_t WEBSERIAL_HTML_SIZE = ${GZIPPED_INDEX.length};
const uint8_t WEBSERIAL_HTML[] PROGMEM = { 
${ addLineBreaks(GZIPPED_INDEX) }
};
`;
  
        fs.writeFileSync(path.resolve(__dirname, SAVE_PATH+'/FlexConsolePage.h'), FILE);
        console.log(`[finalize.js] Compressed Bundle into FlexConsolePage.h header file | Total Size: ${(GZIPPED_INDEX.length / 1024).toFixed(2) }KB`)
    }catch(err){
        return console.error(err);
    }
  })();