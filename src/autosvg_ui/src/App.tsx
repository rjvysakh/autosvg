import React, {FormEvent} from "react";
import cx from "classnames";
import {Button, ButtonGroup, Card, Classes, Elevation, FileInput, Label, Slider} from "@blueprintjs/core";

import s from "./App.module.scss";

function downloadBlobUrl(blobUrl: string) {
    const a = document.createElement('a');
    a.setAttribute('download', 'output.svg');
    a.setAttribute('href', blobUrl);
    a.click();
}

function convertImageToSvg(canvas: HTMLCanvasElement, kColors: number, sharpness: number) {
    const ctx = canvas.getContext("2d");
    if (ctx == null) {
        return;
    }
    //@ts-ignore
    const _Module = Module;
    const AutosvgWASM = _Module.AutosvgWASM;

    const inst = new AutosvgWASM();

    const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
    const imageDataBuffer = imageData.data.buffer;
    const rows = canvas.height;
    const cols = canvas.width;
    const byteCounts = imageDataBuffer.byteLength;
    const dataPtr = _Module._malloc(byteCounts);
    const dataOnHeap = new Uint8ClampedArray(
        _Module.HEAPU8.buffer,
        dataPtr,
        byteCounts
    );
    dataOnHeap.set(imageData.data);

    inst.loadImage(dataOnHeap.byteOffset, rows, cols);

    const svg = inst.convertToSvg(kColors, sharpness);
    const blob = new Blob([svg], {
        type: 'image/svg+xml'
    });
    const blobUrl = URL.createObjectURL(blob);
    console.log(blobUrl);
    _Module._free(dataPtr);
    return blobUrl;
}

function App() {
    const [image, changeImage] = React.useState();
    const [svgBlob, changeBlobUrl] = React.useState();
    const [kColor, handleKColorChange] = React.useState(2);
    const [sharpness, handleSharpnessChange] = React.useState(4);
    const [memCanvas] = React.useState(document.createElement("canvas"));

    async function onFileChange(event: FormEvent<HTMLInputElement>) {
        // @ts-ignore
        const [file] = event.target.files;
        const canvas = memCanvas;
        const imageURL = URL.createObjectURL(file);
        const dummyImg = new Image();
        const ctx = canvas.getContext("2d");
        if (ctx == null) {
            return;
        }
        await new Promise((resolve) => {
            dummyImg.onload = () => {
                canvas.width = dummyImg.width;
                canvas.height = dummyImg.height;
                ctx.drawImage(dummyImg, 0, 0);
                resolve();
            };
            dummyImg.src = imageURL;
        });
        changeImage(imageURL);
    }

    function handleConvert() {
        const blobUrl = convertImageToSvg(memCanvas, kColor, sharpness);
        changeBlobUrl(blobUrl);
    }

    function handleDownload() {
        downloadBlobUrl(svgBlob);
    }

    return (
        <div className={cx(Classes.DARK, "container", s.mainContainer)}>
            <div className="row container">
                <Card
                    elevation={Elevation.TWO}
                    className={cx("col-6", s.card)}
                >
                    <h6>Input Image</h6>
                    <img src={image} className={cx(s.image)}/>
                </Card>
                <Card
                    elevation={Elevation.TWO}
                    className={cx("col-6", s.card)}
                >
                    <h6>Output Image</h6>
                    <img src={svgBlob} className={cx(s.image)}/>
                </Card>
            </div>
            <div className="padding-1rme">
                <h4>Settings</h4>
                <div className="padding-top-1-5rem padding-bottom-1-5rem">
                    <Label>
                        Details
                    </Label>
                    <Slider
                        min={2}
                        max={12}
                        stepSize={1}
                        labelStepSize={2}
                        onChange={handleKColorChange}
                        value={kColor}
                    />
                    <Label>
                        Sharpness
                    </Label>
                    <Slider
                        min={4}
                        max={48}
                        stepSize={1}
                        labelStepSize={12}
                        onChange={handleSharpnessChange}
                        value={sharpness}
                    />
                </div>
                <ButtonGroup className={cx("padding-top-1rem")}>
                    <FileInput text="Choose file..." onInputChange={onFileChange}/>
                    <Button icon="tick-circle" onClick={handleConvert}>Convert</Button>
                    <Button icon="download" onClick={handleDownload}>Download</Button>
                </ButtonGroup>
            </div>
        </div>
    );
}

export default App;
