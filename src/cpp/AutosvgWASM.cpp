//
// Created by Anuj Kosambi on 12/05/20.
//

#include <core/Operations.hpp>
#include "AutosvgWASM.hpp"
#include <utils/CurveUtils.hpp>
#include <string>

using namespace std;
using namespace cv;
using namespace emscripten;

namespace pi {
    void AutosvgWASM::loadImage(uintptr_t buffer, int rows, int cols) {
        try {
            imagePixels = reinterpret_cast<unsigned int *>(buffer);
            img = new cv::Mat(rows, cols, CV_8UC4, imagePixels);

            Operations::sharpen(img, img);

            cv::cvtColor(*img, *img, COLOR_RGBA2RGB);
        } catch (const char *message) {
            cout << "Error in loading image : %s" << message << endl;
        }
    }

    string AutosvgWASM::convertToSvg() {
        SegmentedEdgeResult result = Operations::findColorSegmentedEdge(img, img, 4);

        const vector<Pixel> colors = result.colors;
        const vector<Contour> edges = result.edges;
        vector<Curve> curve = CurveUtils::convertContoursToBezierCurves(edges);

        const vector<SVGParam> params = {
                {"width",  to_string(img->cols).c_str()},
                {"height", to_string(img->rows).c_str()},
                {"xmlns",  "http://www.w3.org/2000/svg"}
        };
        string svg = CurveUtils::createSvgFromBezierCurves(curve, colors, params);
        cv::cvtColor(*img, *img, COLOR_RGB2RGBA);
        memcpy(imagePixels, img->data, img->rows * img->cols * sizeof(int));
        return svg;
    }
}