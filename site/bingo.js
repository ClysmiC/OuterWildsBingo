function assert(value) {
	if (!value) {
		debugger;
		alert("Assertion failed");
	}
}

function cellsFromHeaderId(headerId) {
	let cells = [];

	if (headerId === "tlbr") {
		cells.push(document.getElementById("0_0"));
		cells.push(document.getElementById("1_1"));
		cells.push(document.getElementById("2_2"));
		cells.push(document.getElementById("3_3"));
		cells.push(document.getElementById("4_4"));
	}
	else if (headerId === "bltr") {
		cells.push(document.getElementById("4_0"));
		cells.push(document.getElementById("3_1"));
		cells.push(document.getElementById("2_2"));
		cells.push(document.getElementById("1_3"));
		cells.push(document.getElementById("0_4"));
	}
	else if (headerId.startsWith("col")) {
		let col = headerId.slice(-1);
		col = parseInt(col) - 1;

		for (let i = 0; i < 5; i++) {
			cells.push(document.getElementById(i + "_" + col));
		}
	}
	else if (headerId.startsWith("row")) {
		let row = headerId.slice(-1);
		row = parseInt(row) - 1;

		for (let i = 0; i < 5; i++) {
			cells.push(document.getElementById(row + "_" + i));
		}
	}
	else {
		assert(false);
	}

	return cells;
}

function initLayoutAndHandlers() {
	let seed;

	// Get random seed from URL or generate one

	if (window.location.href.includes("?seed=")) {
		let seedIndex = window.location.href.indexOf("?seed=");
		seedIndex += "?seed=".length;

		seed = parseInt(window.location.href.slice(seedIndex));
	}
	else {
		let generatedSeed = Math.floor(Math.random() * 1000000000);
		window.location.replace(window.location.href + "?seed=" + generatedSeed);
	}

	// Set cell size

	{
		let dimension = 0.7 * Math.min(window.innerWidth, window.innerHeight);
		dimension = Math.max(400, dimension);
		dimension = Math.min(800, dimension);

		let cellDimension = dimension / 5.0;

		let cells = document.getElementsByTagName("td");
		for (let i = 0; i < cells.length; i++) {
			cells[i].style.width = cellDimension + "px";
			cells[i].style.height = cellDimension + "px";
		}
	}

	// Resize right panel

	let panelCard = document.getElementById("panelCard");
	let panelRight = document.getElementById("panelRight");

	panelRight.style.height = panelCard.offsetHeight + 0.5 * (window.innerHeight - panelCard.offsetHeight) + "px";
	panelRight.style.width = (window.innerWidth - panelCard.offsetWidth - 50) + "px";

	// Initialize new card button

	document.getElementById("btnNewCard").addEventListener("click", function () {
		let locationSubstring = window.location.href;
		if (window.location.href.includes("?seed=")) {
			locationSubstring = locationSubstring.substring(0, window.location.href.indexOf("?seed="));
		}
		
		let generatedSeed = Math.floor(Math.random() * 1000000000);
		window.location.replace(locationSubstring + "?seed=" + generatedSeed);

		// TODO: Kick off new generation. Was previously relying on hard redirect.
	});

	// Initialize row/column headers

	let headerSelected = null;
	let headers = document.getElementsByTagName("th");
	for (let i = 0; i < headers.length; i++) {
		let cells = cellsFromHeaderId(headers[i].id);

		headers[i].addEventListener("mouseenter", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.add("fakeHover");
			}
		});

		headers[i].addEventListener("mouseout", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.remove("fakeHover");
				cells[i].classList.remove("preselected");
			}
		});

		headers[i].addEventListener("mousedown", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.add("preselected");
			}
		});

		headers[i].addEventListener("mouseup", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.remove("preselected");
			}
		});

		headers[i].addEventListener("click", function () {
			if (headerSelected !== null) {

				assert(headerSelected.classList.contains("selected"));
				headerSelected.classList.remove("selected");

				let cellsPrev = cellsFromHeaderId(headerSelected.id);
				for (let i = 0; i < cellsPrev.length; i++) {
					cellsPrev[i].classList.remove("selected");
				}
			}

			if (headerSelected === this) {
				headerSelected = null;
			}
			else {
				headerSelected = this;

				assert(!headerSelected.classList.contains("selected"));
				headerSelected.classList.add("selected");

				for (let i = 0; i < cells.length; i++) {
					cells[i].classList.add("selected");
				}
			}
		});
	}

	// Init click listener for goals

	let goalCells = [];
	for (let i = 0; i < 5; i++) {
		goalCells.push([]);

		for (let j = 0; j < 5; j++) {
			let cell = document.getElementById(i + "_" + j);
			cell.addEventListener("click", function () {
				if (this.classList.contains("greenCell")) {
					this.classList.add("redCell");
					this.classList.remove("greenCell");
				}
				else if (this.classList.contains("redCell")) {
					this.classList.remove("redCell");
				}
				else {
					this.classList.add("greenCell");
				}
			});
		}
	}

	// Make visible

	document.getElementById("panelMain").style.visibility = "visible";
}

function initGoals() {

}

initLayoutAndHandlers();
initGoals();