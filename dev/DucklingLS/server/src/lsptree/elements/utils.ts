
export class UnpackingError extends Error {
	constructor(message: string) {
		super(message);
		this.name = "UnpackingError";
	}
}

export const unpackJSONObject = (json: {[key: string]: JSON}, key: string) => {
	const keys = Object.keys(json);
	if (keys.length !== 1 || keys[0] !== key) {
		throw new UnpackingError("Invalid JSON for " + key + ": " + JSON.stringify(json));
	}
	return json[key];
};
