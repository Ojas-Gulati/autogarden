import React, { useRef } from 'react';

export class Post extends React.Component {
	constructor(props) {
		super(props);
		this.begin = this.begin.bind(this);
		this.succeed = this.succeed.bind(this);
		this.fail = this.fail.bind(this);
		this.reset = this.reset.bind(this);
		this.state = {
			loading: false,
			failed: false,
			success: false
		};
	}

	begin() {
		this.setState({
			loading: true,
			failed: false,
			success: false
		});
	}

	succeed() {
		this.setState({
			loading: false,
			failed: false,
			success: true
		});
	}

	fail() {
		this.setState({
			loading: false,
			failed: true,
			success: false
		});
	}

	reset() {
		this.setState({
			loading: false,
			failed: false,
			success: false
		});
	}

	render() {
		return (
			<span>
				{ this.state.loading ? <i className="material-icons">sync</i> : null}
				{ this.state.failed ? <i className="material-icons text-danger">close</i> : null}
				{ this.state.success ? <i className="material-icons text-success">done</i> : null}
			</span>
		)
	}
}

export class Editable extends React.Component {
	constructor(props) {
		super(props);
		this.state = {
			editing: false,
			val: props.value
		};
	}

	toggleEdit(val) {
		let newediting = !this.state.editing;
		if (val === true || val === false) {
			newediting = val;
		}

		this.setState({ editing: newediting })

		if (!newediting) {
			this.props.update(this.state.val);
		}
	}

	render() {
		let inpelem = null;
		if (this.state.editing) {
			inpelem = (<input onChange={(event) => { this.setState({ val: event.target.value }) }} value={this.state.val} />)
		}
		else {
			inpelem = this.props.value
		}

		return (
			<span>{inpelem} <i className="material-icons text-muted" onClick={() => { this.toggleEdit(null) }}>{this.state.editing ? "check" : "edit"}</i></span>
		);
	}
}

export class Flash extends React.Component {
	constructor(props) {
		super(props);
		this.state = {
			flash: false
		}
		this.flashtimeout;
		this.flash = this.flash.bind(this);
	}

	flash() {
		console.log("flashed")
		this.setState({
			flash: true
		}, () => {
			this.flashtimeout = setTimeout(() => {
				this.setState({ flash: false });
			}, 50)
		})
	}

	componentWillUnmount() {
		clearTimeout(this.flashtimeout);
	}

	render() {
		return (<i className={"material-icons " + (this.state.flash ? "" : "flash-fade")}>{this.props.icon}</i>);
	}
}

export class Popup extends React.Component {
	constructor(props) {
		super(props);
		this.showPopup = this.showPopup.bind(this);
		this.hidePopup = this.hidePopup.bind(this);
		this.fadingtimeout = null;
		this.state = {
			fading_up: false,
			fading_down: true,
			faded: true
		}
	}

	showPopup() {
		if (this.state.faded) {
			this.setState({ faded: false, fading_down: true, fading_up: false });
			setTimeout(() => {
				this.setState({ faded: false, fading_up: true, fading_down: false });
			}, 50);
		}
		if (this.props.shown) {
			this.props.shown();
		}
	}

	hidePopup() {
		if (!this.state.faded) {
			this.setState({ faded: false, fading_up: true, fading_down: false });
			setTimeout(() => {
				this.setState({ faded: false, fading_down: true, fading_up: false });
			}, 50);
			setTimeout(() => {
				this.setState({ faded: true, fading_down: true, fading_up: false });
			}, 1000);
		}
		if (this.props.hidden) {
			this.props.hidden();
		}
	}

	render() {
		return (
			<div className={"popup " + (this.state.faded ? "ag-faded" : "") + " " + (this.state.fading_up ? "ag-fading_up" : "") + " " + (this.state.fading_down ? "ag-fading_down" : "")}>
				<div className="popupX">
					<i className="float-right ml-auto material-icons cursor-pointer font-size-medium" onClick={this.hidePopup}>close</i>
				</div>
				<div className="overflow-overlay floating-scrollbar p-2" style={{"max-height": "88vh"}}>
					{this.props.children}
				</div>
			</div>
		)
	}
}